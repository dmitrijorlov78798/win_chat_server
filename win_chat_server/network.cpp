#include "network.h"

#ifdef __WIN32__
std::unordered_set<unsigned> g_journal; // журнал для регистрации объектов с сетевым взаимодействием
#endif

void network::RAII_OSsock::registration()
{
#ifdef __WIN32__
    if (g_journal.empty()) // если интерфейс еще не открывали и количество пользователей = 0
        if (WSAStartup(MAKEWORD(2, 2), &wsdata))// открыть интерфейс
            logger.doLog("RAII_OSsock - WSAStartup ", GetError());// логгируем ошибку

    objectID = ++countWSAusers; // создаем уникальный ID
    g_journal.insert(objectID); // регистрируемся 
#endif
}

/// <summary>
/// конструктор по умолчанию
/// </summary>
network::RAII_OSsock::RAII_OSsock(log_t& logger) : logger(logger)
{
    registration();
}

network::RAII_OSsock::RAII_OSsock(const RAII_OSsock& rvalue) : logger(rvalue.logger)
{
    registration();
}

network::RAII_OSsock::RAII_OSsock(RAII_OSsock&& rvalue) noexcept : logger(rvalue.logger)
{
    registration();
}

network::RAII_OSsock::~RAII_OSsock()
{
#ifdef __WIN32__
    g_journal.erase(objectID); // удаляем свою запись, если уже не сделали этого раньше (Move семантика)

    if (g_journal.empty()) // как только работу закончил последний юзер
        WSACleanup(); // закрыть интерфейс
#endif
}

/// <summary>
/// Метод вывода номера последней ошибки
/// </summary>
/// <returns> номер последней ошибки </returns>
int network::RAII_OSsock::GetError()
{
#ifdef __WIN32__
    return WSAGetLastError();
#else
    return errno;
#endif
}

/// <summary>
/// Метод установки опций для сокета
/// </summary>
/// <param name="socket"> - дескриптор сокета </param>
/// <param name="option"> - опция</param>
/// <param name="logger"> - объект для логгированя </param>
/// <returns> 1 - успех </returns>
bool network::RAII_OSsock::setSocketOpt(SOCKET sock, int option, log_t& logger)
{
    bool result = false;

    switch (option)
    {
    case option_t::NON_BLOCK: // опция по устаеновке неблокирующего сокета
    {
#ifdef __WIN32__
        u_long mode = 0;
        if (ioctlsocket(sock, FIONBIO, &mode))
            logger.doLog("RAII_OSsock - ioctlsocket ", GetError());// логгируем ошибку
        else
            result = true;
#else
        if (fcntl(sock, F_SETFL, O_NONBLOCK))
            logger.doLog("RAII_OSsock - ioctl ", GetError());// логгируем ошибку
        else
            result = true;
#endif
    }
    default:
        break;
    }

    return result;
}

#ifdef __WIN32__
WSADATA network::RAII_OSsock::wsdata;
int network::RAII_OSsock::countWSAusers = 0;
#endif


/// <summary>
/// Метод перезаписи структуры описывающей сокет. После перезаписи необходимо обновить информцию о сокете вызвав UpdateSockInfo()
/// </summary>
/// <returns> указатель на структуру описывающую сокет </returns>
sockaddr* network::sockInfo_t::setSockAddr()
{
    return &Addr;
}

/// <summary>
/// Метод обновления иноформации о сокете, вызывается после обновления Addr метоом setSockAddr()
/// </summary>
void network::sockInfo_t::UpdateSockInfo()
{
    // чистим старую информацию о привязке сокета
    IP_port.first.clear();
    IP_port.second = 0;

    sockaddr_in AddrIN; // Структура описывает сокет для работы с протоколами IP
    memset(&AddrIN, 0, SizeAddr());
    memmove(&AddrIN, &Addr, SizeAddr()); //Addr ==>> AddrIN

    char bufIP[32];// буфер для строки IP
    memset(bufIP, '\0', sizeof(bufIP));

    // Функция InetNtop преобразует интернет-адрес IPv4 или IPv6 в строку в стандартном формате Интернета
    if (inet_ntop(AF_INET, &AddrIN, bufIP, sizeof(bufIP)))
    {
        IP_port.first.assign(bufIP);
        IP_port.second = ntohs(AddrIN.sin_port);
    }
    else // логгируем ошибку
        logger.doLog("inet_ntop fail", GetError());
}

/// <summary>
/// Метод обновления иноформации о сокете, вызывается после обновления Addr
/// </summary>
/// <param name="ip"> IP адресс в формате "хххх.хххх.хххх.хххх" </param>
/// <param name="port"> номер порта </param>
void network::sockInfo_t::UpdateSockInfo(std::string ip, unsigned short port)
{
    IP_port.first.swap(ip);
    IP_port.second = port;
}

/// <summary>
/// Конструктор с одним параметром
/// </summary>
/// <param name="logger"> - объект для логгирования ошибок </param>
network::sockInfo_t::sockInfo_t(log_t& logger) : RAII_OSsock(logger), logger(logger)
{
    memset(&Addr, 0, sizeof(Addr));
    IP_port.first.clear();
    IP_port.second = 0;
    sizeAddr = sizeof(Addr);
}

/// <summary>
/// Конструктор с 2-я параметрами
/// </summary>
/// <param name="ip"> - IP адресс в формате "хххх.хххх.хххх.хххх" </param>
/// <param name="port"> - номер порта </param>
/// <param name="logger"> - объект для логгирования ошибок </param>
network::sockInfo_t::sockInfo_t(std::string ip, unsigned short port, log_t& logger) : sockInfo_t(logger)
{
    setSockInfo(ip, port);
}

/// <summary>
/// деструктор
/// </summary>
network::sockInfo_t::~sockInfo_t()
{
    memset(&Addr, 0, sizeof(Addr));
    IP_port.first.clear();
    IP_port.second = 0;
}

/// <summary>
/// Метод установки информации о сокете
/// </summary>
/// <param name="ip"> - IP адресс в формате "хххх.хххх.хххх.хххх" </param>
/// <param name="port"> - номер порта</param>
/// <returns> true - успех; false - неудача </returns>
bool network::sockInfo_t::setSockInfo(std::string ip, unsigned short port)
{
    bool result = false; // результат

    sockaddr_in AddrIN; // Структура описывает сокет для работы с протоколами IP
    AddrIN.sin_family = AF_INET; // Семейство адресов
    AddrIN.sin_port = htons(port); // Номер порта транспортного протокола
    // Функция htons возвращает значение в порядке байтов сети TCP/IP

    //AddrIN.sin_addr - Структура IN_ADDR , содержащая транспортный адрес IPv4.
    // константы:
    // NADDR_ANY все адреса локального хоста(0.0.0.0);
    // INADDR_LOOPBAC адрес loopback интерфейса(127.0.0.1);
    // INADDR_BROADCAST широковещательный адрес(255.255.255.255)
    int inet_pton_state = inet_pton(AF_INET, ip.c_str(), &AddrIN.sin_addr);
    // Функция InetPton преобразует сетевой адрес IPv4 или IPv6 в стандартной форме
    //представления текста в числовую двоичную форму
    // возврат 1 - удача, 0 - неверная строка, -1 - ошибка

    // проверяем результат работы функции
    if (inet_pton_state == 1) // все отлично
    {
        memset(&Addr, 0, SizeAddr());
        memmove(&Addr, &AddrIN, SizeAddr()); //AddrIN ==>> Addr
        result = true;
        UpdateSockInfo(ip, port);
    }
    else if (inet_pton_state == 0) // неверно задан IP
        logger.doLog(std::string("setSockAddr Fail, invalid IP: ") + ip);
    else //inet_pton fail
        logger.doLog("inet_pton Fail,IP: " + ip, GetError());

    if (result)// вывод отладочной информации что присвоение прошло успешно
    {
        DEBUG_TRACE(logger, "setSockAddr: -> OK")
    }

    return result;
}

/// <summary>
/// Метод установки информации о сокете
/// </summary>
/// <param name="sockInfo"> - структура описывающая сокет для работы с протоколами IP </param>
void network::sockInfo_t::setSockInfo(const sockInfo_t& sockInfo)
{
    memset(&Addr, 0, SizeAddr());
    memmove(&Addr, &sockInfo.Addr, SizeAddr());
    IP_port = sockInfo.IP_port;
}

/// <summary>
/// Метод возвратата константного указателя на структуру описывающую сокет
/// </summary>
/// <returns> константный указатель на структуру описывающую сокет </returns>
const sockaddr* network::sockInfo_t::getSockAddr() const
{
    return &Addr;
}

network::sockInfo_t network::sockInfo_t::GetSockInfo() const
{
    return *this;
}

/// <summary>
/// Метод возврата размера структуры описывающей сокет
/// </summary>
/// <returns> размер структуры описывающей сокет </returns>
size_t network::sockInfo_t::SizeAddr() const
{
    return sizeAddr;
}

/// <summary>
/// Метод возврата IP
/// </summary>
/// <returns> IP адресс в формате "хххх.хххх.хххх.хххх" </returns>
std::string network::sockInfo_t::GetIP() const
{
    return IP_port.first;
}

/// <summary>
/// Метод возврата номера порта
/// </summary>
/// <returns> номер порта </returns>
unsigned short network::sockInfo_t::GetPort() const
{
    return IP_port.second;
}

/// <summary>
/// оператор сравнения
/// </summary>
/// <param name="rValue"> - правостороннее значение </param>
/// <returns> 1 - объекты равны </returns>
bool network::sockInfo_t::operator == (const sockInfo_t& rValue) const
{// сравниваем адреса структур sockaddr (точно уникальные)
    return getSockAddr() == rValue.getSockAddr();
}

/// <summary>
/// оператор сравнения
/// </summary>
/// <param name="rValue"> - правостороннее значение </param>
/// <returns> 1 - объекты не равны </returns>
bool network::sockInfo_t::operator != (const sockInfo_t& rValue) const
{
    return !(*this == rValue);
}

/// <summary>
/// Метод замены дескриптора сокета (для внутреннего пользования серверным сокетом TCP)
/// </summary>
/// <param name="socket"> - дескриптор сокета </param>
/// <param name="nonBlock"> - флаг неблокирующего сокета </param>
/// <returns> true - при удачной замене, false - при неудачной </returns>
bool network::socket_t::SetSocket(SOCKET socket, bool nonBlock)
{
    bool result = false;
    // если аргумент валидный
    if (socket != INVALID_SOCKET)
        if (Close()) // закрываем старый сокет
        {
            Socket = socket;
            this->nonBlock = nonBlock; // accept вернет дефолтный блокирующий сокет
            // обновляем информацию о сокете
            int sizeAddr = SizeAddr();
            if (!getsockname(Socket, setSockAddr(), &sizeAddr))
                UpdateSockInfo();// обязательно после setSockAddr()
            else
                logger.doLog("getsockname fail", GetError());

            result = true;
        }

    return result;
}

/// <summary>
/// Метод закрытия сокета
/// </summary>
/// <returns> true - сокет закрыт </returns>
bool network::socket_t::Close()
{
    if (CheckValidSocket(false)) // если сокет валидный
        if (CLOSE_SOCKET(Socket)) // закрываем
            logger.doLog("closesocket fail", GetError());
        else
        {
            Socket = INVALID_SOCKET; // обнуляем дескриптор
            UpdateSockInfo("", 0); // и информацию
        }

    return Socket == INVALID_SOCKET;
}

void network::socket_t::Shutdown()
{
    if (shutdown(Socket, SHUT) < 0 && GetError() != error_t::SOCKET_NON_CONNECTED)
        logger.doLog("socket_t::Shutdown() fail, errno: ", GetError());
}

/// <summary>
/// назначает внешний адрес, по которому его будут находить транспортные протоколы по
/// заданию подключающихся процессов, а также назначает порт, по которому эти
/// подключающиеся процессы будут идентифицировать процесс-получатель.
/// Предпологается что информация о привязке сокета уже записана.
/// </summary>
/// <returns> true - удача, false - неудача </returns>
bool network::socket_t::Bind()
{
    bool result = false;//результат

    if (CheckValidSocket() && !IP_port.first.empty() && IP_port.second != 0) // Если задана информация о привязке сокета
    {
        result = (bind(Socket, getSockAddr(), SizeAddr()) == 0); // привязываем его к IP и порту
        if (result) // Логгируем результат
            DEBUG_TRACE(logger, "bind -> ok " + IP_port.first + '.' + std::to_string(IP_port.second));
        else
            logger.doLog("bind -> fail " + IP_port.first + '.' + std::to_string(IP_port.second), GetError());
    }
    else // Если вызвали Bind() не установив информацию о привязке сокета
        logger.doLog("bind invalid IP_port");

    return result;
}

/// <summary>
/// назначает внешний адрес, по которому его будут находить транспортные протоколы по
/// заданию подключающихся процессов, а также назначает порт, по которому эти
/// подключающиеся процессы будут идентифицировать процесс-получатель.
/// </summary>
/// <param name="ip"> - IP адресс в формате "хххх.хххх.хххх.хххх" </param>
/// <param name="port"> - номер порта </param>
/// <returns> 1 - выполнено </returns>
bool network::socket_t::Bind(std::string ip, unsigned short port)
{
    bool result = (setSockInfo(ip, port));
    result = result ? Bind() : false;
    return result;
}

/// <summary>
/// Контсруктор с одним параметром, для создания пустого объекта без валидного сокета
/// </summary>
/// <param name="logger"> - объект для логгирования </param>
network::socket_t::socket_t(log_t& logger) : sockInfo_t(logger), Socket(INVALID_SOCKET), nonBlock(false)
{}

/// <summary>
/// Конструктор с 4-я параметрами
/// </summary>
/// <param name="af"> - Семейство адресов: сокеты могут работать с большим семейством адресов. Наиболее частое семейство – IPv4.
///  Указывается как AF_INET </param>
/// <param name="type"> - ип сокета: обычно задается тип транспортного протокола TCP (SOCK_STREAM) или UDP (SOCK_DGRAM).
///  Но бывают и так называемые "сырые" сокеты, функционал которых сам программист определяет в процессе использования.
///  Тип обозначается SOCK_RAW </param>
/// <param name="protocol"> - Тип протокола: необязательный параметр, если тип сокета указан как TCP или UDP – можно передать значение 0.
/// </param>
/// <param name="logger"> - объект для логгирования ошибок </param>
network::socket_t::socket_t(int af, int type, int protocol, log_t& logger) : socket_t(logger)
{
    Socket = socket(af, type, protocol); // привязывает созданный сокет к заданной параметрами транспортной инфраструктуре сети
    CheckValidSocket(); // проверяем валидность сокета
}

/// <summary>
///  Конструктор с 6 параметрами
/// </summary>
/// <param name="af"> - Семейство адресов: сокеты могут работать с большим семейством адресов. Наиболее частое семейство – IPv4.
///  Указывается как AF_INET </param>
/// <param name="type"> - ип сокета: обычно задается тип транспортного протокола TCP (SOCK_STREAM) или UDP (SOCK_DGRAM).
///  Но бывают и так называемые "сырые" сокеты, функционал которых сам программист определяет в процессе использования.
///  Тип обозначается SOCK_RAW </param>
/// <param name="protocol"> - Тип протокола: необязательный параметр, если тип сокета указан как TCP или UDP – можно передать значение 0.
/// </param>
/// <param name="ip"> - IP адресс в формате "хххх.хххх.хххх.хххх" </param>
/// <param name="port"> - номер порта </param>
/// <param name="logger"> - объект для логгирования ошибок </param>
network::socket_t::socket_t(int af, int type, int protocol, std::string ip, unsigned short port, log_t& logger) : socket_t(af, type, protocol, logger)
{
    Bind(ip, port);
}

/// <summary>
///  Конструктор с 5 параметрами
/// </summary>
/// <param name="af"> - Семейство адресов: сокеты могут работать с большим семейством адресов. Наиболее частое семейство – IPv4.
///  Указывается как AF_INET </param>
/// <param name="type"> - ип сокета: обычно задается тип транспортного протокола TCP (SOCK_STREAM) или UDP (SOCK_DGRAM).
///  Но бывают и так называемые "сырые" сокеты, функционал которых сам программист определяет в процессе использования.
///  Тип обозначается SOCK_RAW </param>
/// <param name="protocol"> - Тип протокола: необязательный параметр, если тип сокета указан как TCP или UDP – можно передать значение 0.
/// </param>
/// <param name="sockInfo"> - объект содержащий информацию о сокете
/// <param name="logger"> - объект для логгирования ошибок </param>
network::socket_t::socket_t(int af, int type, int protocol, sockInfo_t sockInfo, log_t& logger) : socket_t(af, type, protocol, logger)
{
    setSockInfo(sockInfo);
    Bind();
}

/// <summary>
/// Метод проверки валидности сокета
/// </summary>
/// <param name="logOn"> - флаг на задание логгирования невалидности сокета </param>
/// <returns> true - сокет валиден, false - сокет не валиден </returns>
bool network::socket_t::CheckValidSocket(bool logOn)
{
    bool result = Socket != INVALID_SOCKET;

    if (!result && logOn) logger.doLog("Socket != INVALID_SOCKET", GetError());

    return result;
}

/// <summary>
/// деструктор
/// </summary>
network::socket_t::~socket_t()
{
    Close();
}

/// <summary>
/// Метод возврата дескриптора сокета
/// </summary>
/// <returns> дескриптор сокета </returns>
SOCKET network::socket_t::getSocket() const
{
    return Socket;
}

/// <summary>
/// установить сокет неблокирующим
/// </summary>
/// <param name="NonBlock"></param>
/// <returns> 1 - сокет не блокирующий </returns>
bool network::socket_t::setNonBlock()
{
    if (!nonBlock && CheckValidSocket(false))
        nonBlock = RAII_OSsock::setSocketOpt(Socket, RAII_OSsock::option_t::NON_BLOCK, logger);
    return nonBlock;
}

/// <summary>
/// Заменить сокет, данный метод нарушает принцип уникальности ресурса, но необходим для серверного сокета для acсept()
/// </summary>
/// <param name="socket"> - новый дескриптор сокета </param>
/// <param name="sockInfo"> - обновленная информация </param>
/// <returns> true - успешная замена </returns>
bool network::TCP_socketClient_t::SetSocket(SOCKET socket, sockInfo_t sockInfo)
{
    bool result = (b_connected = socket_t::SetSocket(socket, false));
    if (result) serverInfo.setSockInfo(sockInfo);
    return result;
}

network::TCP_socketClient_t::~TCP_socketClient_t()
{
    Shutdown();
}

/// <summary>
/// метод переноса уникального ресурса (дескпритор) между сокетами
/// </summary>
/// <param name="source"> - ссылка на сокет источник - после вызова метода не содержит валидный сокет </param>
void network::TCP_socketClient_t::Move(TCP_socketClient_t& source)
{
    if (Close() && socket_t::SetSocket(source.getSocket(), source.nonBlock))
    {
        serverInfo.setSockInfo(source.serverInfo);
        b_connected = source.b_connected;
        source.b_connected = false;
        source.Socket = INVALID_SOCKET;
        source.nonBlock = false;
        source.serverInfo.UpdateSockInfo("", 0);
        source.UpdateSockInfo("", 0);
    }
}

/// <summary>
/// Конструткор с 1 параметром
/// </summary>
/// <param name="logger"> - объект для логгирования </param>
network::TCP_socketClient_t::TCP_socketClient_t(log_t& logger) : socket_t(logger), b_connected(false), serverInfo(logger)
{}

/// <summary>
/// Конструктор с 3 параметрами
/// </summary>
/// <param name="ip_server"> - IP адрес сервера в формате "хххх.хххх.хххх.хххх" </param>
/// <param name="port_server"> - номер порта сервера </param>
/// <param name="logger"> - объект логгирования </param>
network::TCP_socketClient_t::TCP_socketClient_t(std::string ip_server, unsigned short port_server, log_t& logger) : socket_t(AF_INET, SOCK_STREAM, 0, logger), b_connected(false), serverInfo(logger)
{
    if (serverInfo.setSockInfo(ip_server, port_server)) // Если успешно задали информацию о сервере
        Connected(); // устанавливаем соденение с ним
}

/// <summary>
/// Коструктор с 2 параметрами
/// </summary>
/// <param name="serverSockInfo"> - информация о сервере </param>
/// <param name="logger"> - объект логгирования </param>
network::TCP_socketClient_t::TCP_socketClient_t(sockInfo_t serverSockInfo, log_t& logger) : socket_t(AF_INET, SOCK_STREAM, 0, logger), b_connected(false), serverInfo(logger)
{
    serverInfo.setSockInfo(serverSockInfo); // задаем информацию о сервере
    Connected(); // устанавливаем соединение
}

/// <summary>
/// Метод приема сообщений с соединенного сокета с поддержкой контроля размера принятого сообщения
/// </summary>
/// <param name="str_bufer"> - буфер для приема данных </param>
/// <param name="str_EndOfMessege"> - строка символов обозначающая конец сообщения (опционально) </param>
/// <param name="sizeMsg"> - размер ожидаемого сообщения (опционально) </param>
/// <returns> 0 - сообщение принято поностью;
///           N>0 - принято N байт;
///           -1 - системная ошибка;
///           -2 - соединение закрыто или невалидный сокет;
///           -3 - данных на входе нет(неблокирующий сокет)</returns>
int network::TCP_socketClient_t::Recive(std::string& str_bufer, const std::string str_EndOfMessege, const size_t sizeMsg)
{
    int result = -1;
    // если есть соединение
    if (b_connected && CheckValidSocket(false))
    {
        if (!nonBlock) // если мы синхронный сокет
            str_bufer.clear(); // готовим буферный параметр
        
        std::string tempStr(2048, '\0'); // временная строка фиксированного размера для приема данных
        int reciveSize = 0; // размер принятых данных
        bool EOM = str_EndOfMessege.empty() && (sizeMsg == 0); // EndOfMessege признак конца сообщения
        // цикл приема данных
        do {
            reciveSize = recv(Socket, &tempStr[0], tempStr.size(), 0); // Функция служит для чтения данных из сокета.

            if (reciveSize > 0)
            {// если данные есть
                DEBUG_TRACE(logger, "Recive msg: " + tempStr)
                    tempStr.assign(tempStr.c_str()); // избовляемся от лишних '\0';
                str_bufer += tempStr; // добавляем в буфер
                tempStr.clear(); // чистим строку

                if (!str_EndOfMessege.empty())
                { // если задан EOM
                    int pos = str_bufer.size() - str_EndOfMessege.size(); // вычисляем позицию ЕОМ в буфуре
                    if (pos >= 0) // если все валидно
                        EOM = (str_bufer.rfind(str_EndOfMessege) == pos); // находим ЕОМ с конца буфера
                }
                if (sizeMsg != 0) // если задан размер сообщения
                    EOM |= (str_bufer.size() >= sizeMsg); // проверяем, не все ли мы уже получили

                result = EOM ? 0 : reciveSize; // Если приняли все сообщение, то 0, если часть, то кол-во байт
            }
            else if (reciveSize < 0)
            {   // если сокет не блокирующий, проверяем, может просто нет данных
                if (nonBlock && GetError() == error_t::NON_BLOCK_SOCKET_NOT_READY)
                    result = -3; // сокет не блокирующий, нет данных
                else
                {   // если была ошибка, логгируем
                    logger.doLog("TCP_socketClient_t::Recive() fail, errno: ", GetError());
                    result = -1; // системная ошибка
                    b_connected = false; // и разрываем соединение
                }
                break;
            }
            else
            {
                result = -2; // соединение закрыто
                b_connected = false;
                break;
            }

        } while (!EOM && !nonBlock); // выполнять пока не конец данных, либо мы неблокирующий сокет, и запускаем цикл один раз
    }
    else
        result = -2; // соединение закрыто

    return result;
}

/// <summary>
/// Метод отправки сообщений с соединенного сокета с поддержкой контроля размера отправленного сообщения
/// </summary>
/// <param name="str_bufer"> - буфер, содержащий данные для отправки </param>
/// <param name="offset"> - смещение от начала буфера, с которого начинается отправка </param>
/// <returns> 0 - сообщение отправлено поностью;
///           N>0 - отправлено N байт (вместе со смещением);
///           -1 - системная ошибка;
///           -2 - соединение закрыто или невалидный сокет;
///           -3 - сокет не готов к отправке (неблокирующий сокет)</returns>
int network::TCP_socketClient_t::Send(const std::string& str_bufer, const unsigned offset)
{
    int result = -1;
    // если мы подключены
    if (b_connected && CheckValidSocket(false))
    {
        int totalSendSize = str_bufer.size(); // требуемое количество отправленных байт
        int sendSize = offset; // текущее количество отправленных байт
        // цикл отправки
        do {
            int tempSize = send(Socket, &str_bufer[sendSize], totalSendSize - sendSize, 0); // используются для пересылки сообщений в другой сокет
            if (tempSize > 0)
            { // Если что то отправили
                DEBUG_TRACE(logger, std::string(&str_bufer[sendSize], tempSize));
                sendSize += tempSize;
                result = (totalSendSize == sendSize) ? 0 : sendSize; // все ли отправили?
            }
            else if (tempSize < 0)
            {// если сокет не блокирующий, проверяем, может просто нет данных
                if (nonBlock && GetError() == error_t::NON_BLOCK_SOCKET_NOT_READY)
                    result = -3; // сокет не блокирующий, нет данных
                else // если ошибка, логгируем ошибку и разрываем соединение, выходим из цикла
                {
                    logger.doLog("TCP_socketClient_t::Send() fail, errno: ", GetError());
                    result = -1; // системная ошибка
                    b_connected = false; // и разрываем соединение
                }
                break;
            }
            else
            {
                result = str_bufer.empty() ? 0 : -1; // пытались отправить пустое сообщение? у вас получилось
                break;
            }
        } while ((totalSendSize > sendSize) && !nonBlock);// выполнять пока не все данные, либо мы неблокирующий сокет, и запускаем цикл один раз
    }
    else
        result = -2; // соединение закрыто

    return result;
}

/// <summary>
/// Метод подключения сокета к серверному сокету
/// </summary>
/// <returns> 1 - есть подключение </returns>
bool network::TCP_socketClient_t::Connected()
{
    // если сокет валиден
    if (CheckValidSocket(false) && !b_connected)
    {   // функция запускает процесс трехстороннего квитирования - она командует TCP - стеку клиента отправить первый пакет SYN
        if (0 != connect(Socket, serverInfo.getSockAddr(), serverInfo.SizeAddr()))
            logger.doLog("TCP_socketClient_t non connected with server:", GetError());
        else
        {   // если мы успешно подключились, обновляем информацию о себе (нас неявно забиндили)
            b_connected = true;
            UpdateSockInfo();
        }
    }

    return b_connected;
}
/// <summary>
/// метод возврата состояния соединения
/// </summary>
/// <returns> 1 - соединение есть </returns>
bool network::TCP_socketClient_t::GetConnected() const
{
    return b_connected;
}

void network::TCP_socketClient_t::ResetConnected()
{
    b_connected = false;
}

void network::TCP_socketClient_t::Shutdown()
{
    if (b_connected)
    {
        socket_t::Shutdown();
        b_connected = false;
    }
}

/// <summary>
/// конструктор с 3-я параметрами
/// </summary>
/// <param name="ip"> - IP адресс в формате "хххх.хххх.хххх.хххх" </param>
/// <param name="port"> - номер порта </param>
/// <param name="logger"> - объект логгирования </param>
network::TCP_socketServer_t::TCP_socketServer_t(std::string ip, unsigned short port, log_t& logger) : socket_t(AF_INET, SOCK_STREAM, 0, ip, port, logger)
{ //Функция listen помещает сокет в состояние, в котором он прослушивает входящее соединение
    if (CheckValidSocket(false))
        if (0 != listen(Socket, SOMAXCONN))
            this->logger.doLog("TCP_socketServer_t listen fali ", GetError());
}

/// <summary>
/// конструктор с 2-я параметрами
/// </summary>
/// <param name="sockInfo"> - информация о сокете </param>
/// <param name="logger"> - объект логгирования </param>
network::TCP_socketServer_t::TCP_socketServer_t(sockInfo_t sockInfo, log_t& logger) : socket_t(AF_INET, SOCK_STREAM, 0, sockInfo, logger)
{
    if (CheckValidSocket(false))
        if (0 != listen(Socket, SOMAXCONN))
            this->logger.doLog("TCP_socketServer_t listen fali ", GetError());
}

/// <summary>
/// метод добавления подключенных клиентов
/// </summary>
/// <param name="client"> - ссылка на клиента под замену на подключенного клиента </param>
/// <returns> 0 - добавлен подключенный клиент,
///          -1 - системная ошибка,
///          -2 - нет клиентов в очереди на подключение (неблокирующий сокет)</returns>
///			 -3 - невалидный аргумент (существующий сокет) или невалидный серверный сокет
int network::TCP_socketServer_t::AddClient(TCP_socketClient_t& client)
{
    int result = -1;

    if (!client.CheckValidSocket(false) && CheckValidSocket(false))
    {
        sockInfo_t tempInfo(logger); // информация о подключенном сокете
        int sizeAddr = tempInfo.SizeAddr(); // ее размер
        //функция используется сервером для принятия связи на сокет. Сокет должен быть уже слушающим в момент вызова функции.
        //Если сервер устанавливает связь с клиентом, то функция accept возвращает новый сокет-дескриптор, через который
        //и происходит общение клиента с сервером.
        SOCKET tempSocket = accept(Socket, tempInfo.setSockAddr(), &sizeAddr);
        if (tempSocket != INVALID_SOCKET)
        { // если есть какой то сокет, то устанавливаем его
            tempInfo.UpdateSockInfo(); // предварительнообновив информацию о сокете
            if (client.SetSocket(tempSocket, tempInfo))
            { // все получилось? вывод отладки и результата
                DEBUG_TRACE(logger, "addClient success" + tempInfo.GetIP() + std::to_string(tempInfo.GetPort()))
                    result = 0;
            }
            else // иначе проблема в установке сокета
                logger.doLog("fail SetSocket in addClient", GetError());
        } // или мы не получили никакой сокет и если мы неблокирующий сокет и ошибка связана с отсутствием клиентов в очереди на подключение
        else if (GetError() == error_t::NON_BLOCK_SOCKET_NOT_READY && nonBlock)
            result = -2;
        else
            logger.doLog("accept fail", GetError()); // иначе это системная ошибка
    }
    else
        result = -3;

    return result;
}

/// <summary>
/// метод переноса уникального ресурса (дескпритор) между сокетами
/// </summary>
/// <param name="source"> - ссылка на сокет источник - после вызова метода не содержит валидный сокет </param>
void network::UDP_socket_t::Move(UDP_socket_t& source)
{
    if (Close() && socket_t::SetSocket(source.Socket, source.nonBlock))
    {
        lastCommunicationSocket.setSockInfo(source.lastCommunicationSocket);
        source.Socket = INVALID_SOCKET;
        source.nonBlock = false;
        source.lastCommunicationSocket.UpdateSockInfo("", 0);
        source.UpdateSockInfo("", 0);
    }
}

/// <summary>
/// метод установки MTU
/// </summary>
void network::UDP_socket_t::setMTU()
{
    int optlen = sizeof(u32_MTU); // размер опции
    //Функция getsockopt извлекает текущее значение для параметра сокета, связанного с сокетом любого типа, в любом состоянии
    if (getsockopt(Socket, SOL_SOCKET, SO_MAX_MSG_SIZE, (char*)(&u32_MTU), &optlen))
        logger.doLog("getsockopt fail ", GetError());
}

/// <summary>
/// Конструткор с 1 параметром
/// </summary>
/// <param name="logger"> - объект для логгирования </param>
network::UDP_socket_t::UDP_socket_t(log_t& logger) : socket_t(AF_INET, SOCK_DGRAM, 0, logger), lastCommunicationSocket(logger)
{
    setMTU();
}

/// <summary>
/// конструктор с 3-я параметрами
/// </summary>
/// <param name="ip"> - IP адресс в формате "хххх.хххх.хххх.хххх" </param>
/// <param name="port"> - номер порта </param>
/// <param name="logger"> - объект логгирования </param>
network::UDP_socket_t::UDP_socket_t(std::string ip, unsigned short port, log_t& logger) : socket_t(AF_INET, SOCK_DGRAM, 0, ip, port, logger), lastCommunicationSocket(logger)
{
    setMTU();
}

/// <summary>
/// конструктор с 2-я параметрами
/// </summary>
/// <param name="sockInfo"> - информация о сокете </param>
/// <param name="logger"> - объект логгирования </param>
network::UDP_socket_t::UDP_socket_t(sockInfo_t& sockInfo, log_t& logger) : socket_t(AF_INET, SOCK_DGRAM, 0, sockInfo, logger), lastCommunicationSocket(logger)
{
    setMTU();
}

/// <summary>
/// Метод отправки данных в сеть без предварительного соединения
/// </summary>
/// <param name="buffer"> - область с данными для отправки </param>
/// <param name="target"> - информация о сокете приемнике </param>
/// <returns> 0 - отправлено все сообщение;
///         N>0 - отправлено N байт;
///          -1 - системная ошибка;
///          -2 - размер сообщения больше MTU или сокет не валидный
///          -3 - сокет не готов к отправке (неблокирующий сокет) </returns>
int network::UDP_socket_t::SendTo(const std::string& buffer, sockInfo_t target)
{
    int result = -1;
    // проверяем размер сообщения
    if (CheckValidSocket(false) && buffer.size() < MTU())
    { // Функция sendto отправляет данные в определенное место назначения
        int sendSize = sendto(Socket, buffer.c_str(), buffer.size(), 0, target.getSockAddr(), target.SizeAddr());
        // проверяем результат
        if (sendSize > 0)
        { // если есть положительный результат
            result = (sendSize == buffer.size()) ? 0 : sendSize; // если сообщение отправилось полностью, то 0 - все гуд, если нет, то количество переданых байт
            DEBUG_TRACE(logger, "sendto: " + buffer)
        }
        else if (sendSize < 0)
        { // если есть ошибка, проверяем, связана ли она с блокировкой при неблокирующем сокете
            if (GetError() == error_t::NON_BLOCK_SOCKET_NOT_READY && nonBlock)
                result = -3; // сокет не готов (неблокирующий)
            else
                logger.doLog("sendto fail ", GetError()); // иначе системная ошибка
        }
        else if (buffer.empty()) // если пытались отправить ноль? то у нас все получилось
            result = 0;
    }
    else
        result = -2; // размер сообщения больше MTU или сокет не валидный

    // если отправили данные новому сокету, обновляем информацию об этом (как последний набранный номер)
    if (target != lastCommunicationSocket)
        lastCommunicationSocket.setSockInfo(target);

    return result;
}

/// <summary>
/// Метод отправки данных в сеть без предварительного соединения, отправка предыдущему сокету с которым было взаимодействие
/// </summary>
/// <param name="buffer"> - область с данными для отправки </param>
/// <returns> 0 - отправлено все сообщение;
///         N>0 - отправлено N байт;
///          -1 - системная ошибка;
///          -2 - размер сообщения больше MTU или сокет не валидный
///          -3 - сокет не готов к отправке (неблокирующий сокет) </returns>
int network::UDP_socket_t::SendTo(const std::string& buffer)
{
    return SendTo(buffer, lastCommunicationSocket); // отправка предыдущему сокету с которым было взаимодействие
}

/// <summary>
/// Метод отправки данных в сеть без предварительного соединения
/// </summary>
/// <param name="buffer"> - область с данными для отправки </param>
/// <param name="ip"> - IP адресс в формате "хххх.хххх.хххх.хххх" </param>
/// <param name="port"> - номер порта </param>
/// <returns> 0 - отправлено все сообщение;
///         N>0 - отправлено N байт;
///          -1 - системная ошибка;
///          -2 - размер сообщения больше MTU или сокет не валидный
///          -3 - сокет не готов к отправке (неблокирующий сокет) </returns>
int network::UDP_socket_t::SendTo(const std::string& buffer, std::string ip, unsigned short port)
{
    if (lastCommunicationSocket.setSockInfo(ip, port)) // задаем информацию о сокете, если все успешно
        return SendTo(buffer); // оправляем
    else return -1;
}

/// <summary>
/// Метод приема данных из сети без предварительного соединения
/// </summary>
/// <param name="buffer"> - буфер с принятыми данными </param>
/// <param name="str_EndOfMessege"> - признак конца сообщения (опционально) </param>
/// <param name="sizeMsg"> - ожидаемы размер сообщения (опционально) </param>
/// <returns>   0 - все сообщение принято успешно;
///             N>0 - принято N-байт;
///             -1 - системная ошибка;
///             -2 - соеднинение закрыто или сокет не валидный;
///             -3 - сокет не готов (неблокирующий);</returns>
int network::UDP_socket_t::RecvFrom(std::string& buffer, const std::string str_EndOfMessege, const size_t sizeMsg)
{
    int result = -1;

    if (CheckValidSocket(false))
    {
        buffer.clear(); // готовим буфер
        std::string tempStr(2048, '\0'); // временная строка фиксированного размера для приема данных
        int SizeAddr = lastCommunicationSocket.SizeAddr(); // размер структуры Addr
        // Функция recvfrom получает датаграмму и сохраняет исходный адрес
        int recvSize = recvfrom(Socket, &tempStr[0], tempStr.size(), 0, lastCommunicationSocket.setSockAddr(), &SizeAddr);

        if (recvSize > 0)
        { // если результат положителен
            buffer.assign(tempStr.c_str()); // убираем лишние '\0' в конце строки
            DEBUG_TRACE(logger, "recvfrom: " + tempStr)

                bool EOM = str_EndOfMessege.empty() && (sizeMsg == 0);// EndOfMessege признак конца сообщения
            if (!str_EndOfMessege.empty())
            { // если задан EOM
                int pos = buffer.size() - str_EndOfMessege.size(); // вычисляем позицию ЕОМ в буфуре
                if (pos >= 0) // если все валидно
                    EOM = (buffer.rfind(str_EndOfMessege) == pos); // находим ЕОМ с конца буфера
            }
            if (sizeMsg != 0) // если задан размер сообщения
                EOM |= (buffer.size() >= sizeMsg); // проверяем, не все ли мы уже получили

            result = EOM ? 0 : recvSize; // если получили все, то 0, иначе кол-во полученных байт

            lastCommunicationSocket.UpdateSockInfo(); // вызвваем после перезаписи setSockAddr()
        }
        else if (recvSize < 0)
        { // если появилась ошибка, проверяем не связана ли она с блокировкой неблокирующего сокета
            if (GetError() == error_t::NON_BLOCK_SOCKET_NOT_READY && nonBlock)
                result = -3;
            else
                logger.doLog("recfrom fail ", GetError());
        }
        else
            result = -2; // соединение закрыто
    }
    else
        result = -2;

    return result;
}

/// <summary>
/// метод возврата информации о сокете с которым происходило последнее взаимодействие (отправка/прием данных)
/// </summary>
/// <returns> сокет с которым происходило последнее взаимодействие (отправка/прием данных) </returns>
network::sockInfo_t network::UDP_socket_t::GetLastCommunication() const
{
    return lastCommunicationSocket;
}

/// <summary>
/// метод возврата MTU
/// </summary>
/// <returns> максимальный размер сообщения </returns>
unsigned int network::UDP_socket_t::MTU() const
{
    return u32_MTU;
}

/// <summary>
/// метод добавления сокета в один из списков
/// </summary>
/// <param name="m_sock"> - ассоциативный массив для добавления </param>
/// <param name="socket"> - сокет на добавление </param>
/// <returns> 1 - сокет добавлен </returns>
bool network::NonBlockSocket_manager_t::addSocket(std::unordered_map<int, std::weak_ptr<socket_t>>& m_sock, const std::weak_ptr<socket_t>& socket)
{
    bool result = false;
    if (auto ptr = socket.lock())
        if (ptr->CheckValidSocket()) // сокет валидный?
            if (ptr->setNonBlock()) // если сокет неблокирующий
                if (result = m_sock.find(ptr->getSocket()) == m_sock.end()) // и его еще нет в списках
                {
                    m_sock[ptr->getSocket()] = socket; // добавляем его
                    b_change = true; // произошли изменения, нужно менять pollfd
                }

    return result;
}
/// <summary>
/// Метод удаления сокета из списков
/// </summary>
/// <param name="m_sock"> - ассоциативный массив для добавления </param>
/// <param name="socket"> - сокет на удаление </param>
/// <returns> 1 - сокет удален </returns>
bool network::NonBlockSocket_manager_t::deleteSocket(std::unordered_map<int, std::weak_ptr<socket_t>>& m_sock, const std::weak_ptr<socket_t>& socket)
{
    bool result = false;
    // нужно ли делать сокет блокирующим после удаления? по идее нет, удалили от сюда сокет, можем его добавить в другой менеджер
    if (auto ptr = socket.lock())
        if (result = m_sock.find(ptr->getSocket()) != m_sock.end()) // если такой сокет есть в списке
        {
            m_sock.erase(ptr->getSocket()); // удаляем его
            b_change = true; // произошли изменения, нужно менять pollfd
        }

    return result;
}


/// <summary>
/// Метод обновления структуры pollfd
/// </summary>
void network::NonBlockSocket_manager_t::UpdatePollfd()
{
    // считаем количество нулевых указателей
    unsigned countNullptr = 0; // счетчик нулевых указателей
    for (auto it : m_senderSocket) 
        if (it.second.expired())
            ++countNullptr;
    for (auto it : m_readerSocket)
        if (it.second.expired())
            ++countNullptr;
    for (auto it : m_serverSocket)
        if (it.second.expired())
            ++countNullptr;
    for (auto it : m_clientSocket)
        if (it.second.expired())
            ++countNullptr;

    if (b_change || countNullptr > 0)
    { // если были изменения в списках сокетов или какие то указатели стали нулевыми
        b_change = false;
        size_t size = m_senderSocket.size() + m_readerSocket.size() + m_serverSocket.size() + m_clientSocket.size() - countNullptr; // размер массива структур pollfd
        v_fds.clear(); // готовим массив структур pollfd
        v_fds.resize(size);

        size_t indx = 0; // индекс для обхода массива структур pollfd
        std::unordered_map<int, std::weak_ptr<socket_t>>::iterator iter_sender = m_senderSocket.begin(); 
        std::unordered_map<int, std::weak_ptr<socket_t>>::iterator iter_reader = m_readerSocket.begin(); 
        std::unordered_map<int, std::weak_ptr<socket_t>>::iterator iter_server = m_serverSocket.begin();
        std::unordered_map<int, std::weak_ptr<socket_t>>::iterator iter_client = m_clientSocket.begin();
        // цикл для обхода массива структур pollfd
        while (indx < size)
        {
            if (iter_sender != m_senderSocket.end()) // заносим отправителей
            {
                if (auto ptr = iter_sender->second.lock()) // если указатель валидный
                {
                    v_fds[indx].fd = ptr->getSocket(); // дескриптор сокета
                    v_fds[indx].events = POLLOUT; // ожидаемое событие
                    v_fds[indx++].revents = 0; // произошедшее событие
                    ++iter_sender;
                }
                else
                    iter_sender = m_senderSocket.erase(iter_sender); // если указатель нулевой - удаляем
            }
            else if (iter_reader != m_readerSocket.end()) // заносим читателей
            {
                if (auto ptr = iter_reader->second.lock())
                {
                    v_fds[indx].fd = ptr->getSocket();
                    v_fds[indx].events = POLLIN;
                    v_fds[indx++].revents = 0;
                    ++iter_reader;
                }
                else
                    iter_reader = m_readerSocket.erase(iter_reader);
            }
            else if (iter_server != m_serverSocket.end()) // заносим сервера
            {
                if (auto ptr = iter_server->second.lock())
                {
                    v_fds[indx].fd = ptr->getSocket();
                    v_fds[indx].events = POLLIN;
                    v_fds[indx++].revents = 0;
                    ++iter_server;
                }
                else
                    iter_server = m_serverSocket.erase(iter_server);
            }
            else if (iter_client != m_clientSocket.end()) // заносим клиентов
            {
                if (auto ptr = iter_client->second.lock())
                {
                    v_fds[indx].fd = ptr->getSocket();
                    v_fds[indx].events = POLLOUT;
                    v_fds[indx++].revents = 0;
                    ++iter_client;
                }
                else
                    iter_client = m_clientSocket.erase(iter_client);
            }
            else
            {
                v_fds[indx].fd = 0;
                v_fds[indx].events = 0;
                v_fds[indx++].revents = 0;
                b_change = true;
            }
        }

    } // если изменений не было, просто обнуляем произошедшие события
    else
        for (size_t indx = 0; indx < v_fds.size(); ++indx)
            v_fds[indx].revents = 0;

}
/// <summary>
/// функция мультиплексированя неблокирующих сокетов
/// </summary>
/// <param name="timeOut"> - время timeout для функции мультиплексирования </param>
/// <returns> -1 - системная ошибка; 0 - вышел таймаут и событий не произошло; N>0 - кол-во событий </returns>
int network::NonBlockSocket_manager_t::Poll(int timeOut)
{
#ifdef __WIN32__
    return WSAPoll(&v_fds[0], v_fds.size(), timeOut);//Функция WSAPoll возвращает состояние сокета в элементе revents структуры WSAPOLLFD
#else
    return poll(&v_fds[0], v_fds.size(), timeOut);
#endif
}

/// <summary>
/// Конструктор с одним параметром
/// </summary>
/// <param name="logger"> - объект для логиирования </param>
network::NonBlockSocket_manager_t::NonBlockSocket_manager_t(log_t& logger) : RAII_OSsock(logger), b_change(false), logger(logger)
{}
/// <summary>
/// Конструктор с двумя параметрами
/// </summary>
/// <param name="size"> - предварительное количество наблюдаемых сокетов </param>
/// <param name="logger"> - объект для логирования </param>
network::NonBlockSocket_manager_t::NonBlockSocket_manager_t(int size, log_t& logger) : RAII_OSsock(logger), b_change(false), logger(logger)
{
    v_fds.reserve(size);
}
/// <summary>
/// метод добавления отправителя
/// </summary>
/// <param name="socket"> - целевой сокет </param>
/// <returns> 1 - сокет добавлен </returns>
bool network::NonBlockSocket_manager_t::AddSender(const std::weak_ptr<socket_t>& socket) // добавляем сокет
{
    return addSocket(m_senderSocket, socket);
}
/// <summary>
/// метод удаления отправителя
/// </summary>
/// <param name="socket"> - целевой сокет </param>
/// <returns> 1 - сокет удален </returns>
bool network::NonBlockSocket_manager_t::deleteSender(const std::weak_ptr<socket_t>& socket)
{
    return deleteSocket(m_senderSocket, socket);
}
/// <summary>
/// метод добавления читателя
/// </summary>
/// <param name="socket"> - целевой сокет </param>
/// <returns> 1 - сокет добавлен </returns>
bool network::NonBlockSocket_manager_t::AddReader(const std::weak_ptr<socket_t>& socket)
{
    return addSocket(m_readerSocket, socket);
}
/// <summary>
/// метод удаления читателя
/// </summary>
/// <param name="socket"> - целевой сокет </param>
/// <returns> 1 - сокет удален </returns>
bool network::NonBlockSocket_manager_t::deleteReader(const std::weak_ptr<socket_t>& socket)
{
    return deleteSocket(m_readerSocket, socket);
}

/// <summary>
/// метод добавления сервера
/// </summary>
/// <param name="socket"> - целевой сокет </param>
/// <returns> 1 - сокет добавлен </returns>
bool network::NonBlockSocket_manager_t::AddServer(const std::weak_ptr<socket_t>& socket)
{
    return addSocket(m_serverSocket, socket);
}

/// <summary>
/// метод удаления сервера
/// </summary>
/// <param name="socket"> - целевой сокет </param>
/// <returns> 1 - сокет удален </returns>
bool network::NonBlockSocket_manager_t::deleteServer(const std::weak_ptr<socket_t>& socket)
{
    return deleteSocket(m_serverSocket, socket);
}

/// <summary>
/// метод добавления клиента
/// </summary>
/// <param name="socket"> - целевой сокет </param>
/// <returns> 1 - сокет добавлен </returns>
bool network::NonBlockSocket_manager_t::AddClient(const std::weak_ptr<socket_t>& socket)
{
    return addSocket(m_clientSocket, socket);
}

/// <summary>
/// метод удаления клиента
/// </summary>
/// <param name="socket"> - целевой сокет </param>
/// <returns> 1 - сокет удален </returns>
bool network::NonBlockSocket_manager_t::deleteClient(const std::weak_ptr<socket_t>& socket)
{
    return deleteSocket(m_clientSocket, socket);
}

/// <summary>
/// метод проверки готовности отправителя к отправке
/// </summary>
/// <param name="socket"> - целевой сокет </param>
/// <returns> 1 - сокет готов </returns>
bool network::NonBlockSocket_manager_t::GetReadySender(const std::weak_ptr<socket_t>& socket) const
{
    bool result = false;
    if (auto ptr = socket.lock())
        result = m_readySender.find(ptr->getSocket()) != m_readySender.end();
    return result;
}

/// <summary>
/// метод проверки готовности читателя к приему
/// </summary>
/// <param name="socket"> - целевой сокет </param>
/// <returns> 1 - сокет готов </returns>
bool network::NonBlockSocket_manager_t::GetReadyReader(const std::weak_ptr<socket_t>& socket) const
{
    bool result = false;
    if (auto ptr = socket.lock())
        result = m_readyReader.find(ptr->getSocket()) != m_readyReader.end();
    return result;
}

/// <summary>
/// метод проверки наличия запроса на подключения у сервера
/// </summary>
/// <param name="socket"> - целевой сокет </param>
/// <returns> 1 - есть подключение </returns>
bool network::NonBlockSocket_manager_t::GetReadyServer(const std::weak_ptr<socket_t>& socket) const
{
    bool result = false;
    if (auto ptr = socket.lock())
        result = m_readyServer.find(ptr->getSocket()) != m_readyServer.end();
    return result;
}

/// <summary>
/// метод проверки подтверждения подключения к серверу у клиента
/// </summary>
/// <param name="socket"> - целевой сокет </param>
/// <returns> 1 - есть подключение </returns>
bool network::NonBlockSocket_manager_t::GetReadyClient(const std::weak_ptr<socket_t>& socket) const
{
    bool result = false;
    if (auto ptr = socket.lock())
        result = m_readyClient.find(ptr->getSocket()) != m_readyClient.end();
    return result;
}


/// <summary>
/// основной метод работы мультиплесора
/// </summary>
/// <param name="timeOut"> - время ожидания мультиплесора </param>
/// <returns> 1 - принято хотябы одно событие </returns>
bool network::NonBlockSocket_manager_t::Work(const int timeOut)
{// очистка мапов готовых сокетов
    m_readySender.clear();
    m_readyReader.clear();
    m_readyServer.clear();
    m_readyClient.clear();
    // обновление структуры pollfd
    UpdatePollfd();
    size_t size = v_fds.size();
    // вызываем фукцию мультеплексирования
    int resPoll = Poll(timeOut);
    if (resPoll > 0) // если результат положителен
    {  // по всем структурам pollfd
        for (size_t indx = 0; indx < size; ++indx)
            if (v_fds[indx].revents & POLLIN && m_readerSocket.find(v_fds[indx].fd) != m_readerSocket.end()) // нашли событие на читателя
                m_readyReader[v_fds[indx].fd] = m_readerSocket[v_fds[indx].fd];
            else if (v_fds[indx].revents & POLLOUT && m_senderSocket.find(v_fds[indx].fd) != m_senderSocket.end()) // нашли событие на отправителя
                m_readySender[v_fds[indx].fd] = m_senderSocket[v_fds[indx].fd];
            else if (v_fds[indx].revents & POLLIN && m_serverSocket.find(v_fds[indx].fd) != m_serverSocket.end()) // нашли событие на сервер
                m_readyServer[v_fds[indx].fd] = m_serverSocket[v_fds[indx].fd];
            else if (v_fds[indx].revents & POLLOUT && m_clientSocket.find(v_fds[indx].fd) != m_clientSocket.end()) // нашли событие на клиента
                m_readyClient[v_fds[indx].fd] = m_clientSocket[v_fds[indx].fd];
    }
    else if (resPoll < 0) // системная ошибка
        logger.doLog("poll error", GetError());

    return !m_readySender.empty() || !m_readyReader.empty() || !m_readyServer.empty() || !m_readyClient.empty(); // хоть что то добавили в списки?
}

