#pragma once
#ifndef NETWORK_H_
#define NETWORK_H_
#define __WIN32__
#include <vector>
#include <list>
#include <map>
#include <string>

#include "log.h"

#ifdef __WIN32__

#include <unordered_set>
#include <unordered_map>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // !WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <WinSock2.h> // заголовочный файл, содержащий актуальные реализации функций для работы с сокетами
#include <WS2tcpip.h> // заголовочный файл, который содержит различные программные интерфейсы, связанные с работой протокола TCP/IP (переводы различных данных в формат, понимаемый протоколом и т.д.)
#include <iphlpapi.h>
#pragma comment(lib, "Ws2_32.lib") // прилинковать к приложению динамическую библиотеку ядра ОС: ws2_32.dll. Делаем это через директиву компилятору
#define CLOSE_SOCKET(socket) closesocket(socket)
#define SHUT SD_BOTH

#else

#include <sys/types.h>//
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>//
#include <poll.h>
#include <unistd.h>//
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define CLOSE_SOCKET(socket) close(socket)
#define SHUT SHUT_RDWR

#endif

/// <summary>
/// простанство имен классов для работы с сетью
/// </summary>
namespace network
{
    /// <summary>
    /// Класс абстракции от операционной системы
    /// Использует принцип RAII
    /// </summary>
    class RAII_OSsock
    {
    private :
#ifdef __WIN32__
        static WSADATA wsdata; // Структура WSADATA содержит сведения о реализации сокетов Windows.
        static int countWSAusers; // количество пользователей интерфейса
        unsigned objectID; // ID этого объекта
#endif
        void registration();
    protected :
        struct option_t // опции для сокета
        {
            static const int NON_BLOCK = 1; // неблокирующий сокет
        };
        struct error_t // ошибки сокета
        {
#ifdef __WIN32__
            static constexpr int NON_BLOCK_SOCKET_NOT_READY = WSAEWOULDBLOCK; // сокет не блокирующий, не готов к отправке либо приему
            static const int SOCKET_NON_CONNECTED = WSAENOTCONN;
#else
            static const int NON_BLOCK_SOCKET_NOT_READY = EWOULDBLOCK; // сокет не блокирующий, не готов к отправке либо приему
            static const int SOCKET_NON_CONNECTED = ENOTCONN;
#endif
        };
        /// <summary>
        /// конструктор по умолчанию
        /// </summary>
        RAII_OSsock(log_t& logger);

        RAII_OSsock(const RAII_OSsock& rvalue);

        RAII_OSsock(RAII_OSsock&& rvalue) noexcept;

        virtual ~RAII_OSsock();
        /// <summary>
        /// Метод вывода номера последней ошибки
        /// </summary>
        /// <returns> номер последней ошибки </returns>
        int GetError();

        /// <summary>
        /// Метод установки опций для сокета
        /// </summary>
        /// <param name="socket"> - дескриптор сокета </param>
        /// <param name="option"> - опция</param>
        /// <param name="logger"> - объект для логгированя </param>
        /// <returns> 1 - успех </returns>
        bool setSocketOpt(SOCKET socket, int option, log_t& logger);
    protected :
        log_t& logger;
    };

    /// <summary>
    /// класс реализует хранение информации о сокете
    /// </summary>
    class sockInfo_t : public RAII_OSsock
    {
        friend class UDP_socket_t; // для метода RecvFrom
        friend class TCP_socketServer_t; // для метода AddClient
        friend class TCP_socketClient_t; // для метода Move
    protected:
        /// <summary>
        /// Метод перезаписи структуры описывающей сокет. После перезаписи необходимо обновить информцию о сокете вызвав UpdateSockInfo()
        /// </summary>
        /// <returns> указатель на структуру описывающую сокет </returns>
        sockaddr* setSockAddr();

        /// <summary>
        /// Метод обновления иноформации о сокете, вызывается после обновления Addr метоом setSockAddr()
        /// </summary>
        void UpdateSockInfo();

        /// <summary>
        /// Метод обновления иноформации о сокете, вызывается после обновления Addr
        /// </summary>
        /// <param name="ip"> IP адресс в формате "хххх.хххх.хххх.хххх" </param>
        /// <param name="port"> номер порта </param>
        void UpdateSockInfo(std::string ip, unsigned short port);

    public:
        /// <summary>
        /// Конструктор с одним параметром
        /// </summary>
        /// <param name="logger"> - объект для логгирования ошибок </param>
        sockInfo_t(log_t& logger);

        /// <summary>
        /// Конструктор с 2-я параметрами
        /// </summary>
        /// <param name="ip"> - IP адресс в формате "хххх.хххх.хххх.хххх" </param>
        /// <param name="port"> - номер порта </param>
        /// <param name="logger"> - объект для логгирования ошибок </param>
        sockInfo_t(std::string ip, unsigned short port, log_t& logger);

        virtual ~sockInfo_t();

        /// <summary>
        /// Метод установки информации о сокете
        /// </summary>
        /// <param name="ip"> - IP адресс в формате "хххх.хххх.хххх.хххх" </param>
        /// <param name="port"> - номер порта</param>
        /// <returns> true - успех; false - неудача </returns>
        bool setSockInfo(std::string ip, unsigned short port);

        /// <summary>
        /// Метод установки информации о сокете
        /// </summary>
        /// <param name="sockInfo"> - структура описывающая сокет для работы с протоколами IP </param>
        /// <returns></returns>
        void setSockInfo(const sockInfo_t& sockInfo);

        /// <summary>
        /// Метод возвратата константного указателя на структуру описывающую сокет
        /// </summary>
        /// <returns> константный указатель на структуру описывающую сокет </returns>
        const sockaddr* getSockAddr() const;

        sockInfo_t GetSockInfo() const;

        /// <summary>
        /// Метод возврата размера структуры описывающей сокет
        /// </summary>
        /// <returns> размер структуры описывающей сокет </returns>
        size_t SizeAddr() const;

        /// <summary>
        /// Метод возврата IP
        /// </summary>
        /// <returns> IP адресс в формате "хххх.хххх.хххх.хххх" </returns>
        std::string GetIP() const;

        /// <summary>
        /// Метод возврата номера порта
        /// </summary>
        /// <returns> номер порта </returns>
        unsigned short GetPort() const;

        /// <summary>
        /// оператор сравнения
        /// </summary>
        /// <param name="rValue"> - правостороннее значение </param>
        /// <returns> 1 - объекты равны </returns>
        bool operator == (const sockInfo_t& rValue) const;

        /// <summary>
        /// оператор сравнения
        /// </summary>
        /// <param name="rValue"> - правостороннее значение </param>
        /// <returns> 1 - объекты не равны </returns>
        bool operator != (const sockInfo_t& rValue) const;
    protected:
        std::pair<std::string, unsigned short> IP_port; // IP адрес и номер порта
        sockaddr Addr; // стандартная структура для хранения информации о сокете
        size_t sizeAddr; // размер структуры sockaddr
        log_t& logger; // объект для логгирования ошибок
    };

    /// <summary>
    /// Класс реализует хранение и работу сокета
    /// </summary>
    class socket_t : public sockInfo_t
    {// TODO Работа с DNS    getaddrinfo(char const* node, char const* service, struct addrinfo const* hints, struct addrinfo** res)
        friend class NonBlockSocket_manager_t; // менеджер неблокирующмх сокетов, использует setNonBlock
    protected:
        /// <summary>
        /// Метод замены дескриптора сокета (для внутреннего пользования серверным сокетом TCP)
        /// </summary>
        /// <param name="socket"> - дескриптор сокета </param>
        /// <param name="nonBlock"> - флаг неблокирующего сокета </param>
        /// <returns> true - при удачной замене, false - при неудачной </returns>
        bool SetSocket(SOCKET socket, bool nonBlock);

        /// <summary>
        /// Метод закрытия сокета
        /// </summary>
        /// <returns> true - сокет закрыт </returns>
        bool Close();

        void Shutdown();

        /// <summary>
        /// назначает внешний адрес, по которому его будут находить транспортные протоколы по
        /// заданию подключающихся процессов, а также назначает порт, по которому эти
        /// подключающиеся процессы будут идентифицировать процесс-получатель.
        /// Предпологается что информация о привязке сокета уже записана.
        /// </summary>
        /// <returns> true - удача, false - неудача </returns>
        bool Bind();

        /// <summary>
        /// назначает внешний адрес, по которому его будут находить транспортные протоколы по
        /// заданию подключающихся процессов, а также назначает порт, по которому эти
        /// подключающиеся процессы будут идентифицировать процесс-получатель.
        /// </summary>
        /// <param name="ip"> - IP адресс в формате "хххх.хххх.хххх.хххх" </param>
        /// <param name="port"> - номер порта </param>
        /// <returns> 1 - выполнено </returns>
        bool Bind(std::string ip, unsigned short port);

        /// <summary>
        /// Контсруктор с одним параметром, для создания пустого объекта без валидного сокета
        /// </summary>
        /// <param name="logger"> - объект для логгирования </param>
        socket_t(log_t& logger);

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
        socket_t(int af, int type, int protocol, log_t& logger);

        /// <summary>
        ///  Конструктор с 6 параметрами
        /// </summary>
        /// <param name="af"> - Семейство адресов: сокеты могут работать с большим семейством адресов. Наиболее частое семейство – IPv4.
        ///  Указывается как AF_INET </param>
        /// <param name="type"> - тип сокета: обычно задается тип транспортного протокола TCP (SOCK_STREAM) или UDP (SOCK_DGRAM).
        ///  Но бывают и так называемые "сырые" сокеты, функционал которых сам программист определяет в процессе использования.
        ///  Тип обозначается SOCK_RAW </param>
        /// <param name="protocol"> - Тип протокола: необязательный параметр, если тип сокета указан как TCP или UDP – можно передать значение 0.
        /// </param>
        /// <param name="ip"> - IP адресс в формате "хххх.хххх.хххх.хххх" </param>
        /// <param name="port"> - номер порта </param>
        /// <param name="logger"> - объект для логгирования ошибок </param>
        socket_t(int af, int type, int protocol, std::string ip, unsigned short port, log_t& logger);

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
        socket_t(int af, int type, int protocol, sockInfo_t sockInfo, log_t& logger);

        // сокеты - уникальный ресурс, удаляем копирование объектов
        socket_t(const socket_t& sock) = delete;
        socket_t& operator = (const socket_t& sock) = delete;

        /// <summary>
        /// Метод проверки валидности сокета
        /// </summary>
        /// <param name="logOn"> - флаг на задание логгирования невалидности сокета </param>
        /// <returns> true - сокет валиден, false - сокет не валиден </returns>
        bool CheckValidSocket(bool logOn = true);

        virtual ~socket_t();

        /// <summary>
        /// Метод возврата дескриптора сокета
        /// </summary>
        /// <returns> дескриптор сокета </returns>
        SOCKET getSocket() const;

        /// <summary>
        /// установить сокет неблокирующим
        /// </summary>
        /// <returns> 1 - сокет не блокирующий </returns>
        bool setNonBlock();
    protected:
        SOCKET Socket; // дескриптор сокета
        bool nonBlock; // признак неблокирующего сокета
    };

    /// <summary>
    /// TCP клиентский сокет
    /// </summary>
    class TCP_socketClient_t : public socket_t
    {
        friend class TCP_socketServer_t; // даем полную свободу серверному сокету (необходимо для acсept())
    private:
        /// <summary>
        /// Заменить сокет, данный метод нарушает принцип уникальности ресурса, но необходим для серверного сокета для acсept()
        /// </summary>
        /// <param name="socket"> - новый дескриптор сокета </param>
        /// <param name="sockInfo"> - обновленная информация </param>
        /// <returns> true - успешная замена </returns>
        bool SetSocket(SOCKET socket, sockInfo_t sockInfo);

    public:

        virtual ~TCP_socketClient_t();
        /// <summary>
        /// метод переноса уникального ресурса (дескпритор) между сокетами
        /// </summary>
        /// <param name="source"> - ссылка на сокет источник - после вызова метода не содержит валидный сокет </param>
        void Move(TCP_socketClient_t& source);

        /// <summary>
        /// Конструткор с 1 параметром
        /// </summary>
        /// <param name="logger"> - объект для логгирования </param>
        TCP_socketClient_t(log_t& logger);

        /// <summary>
        /// Конструктор с 3 параметрами
        /// </summary>
        /// <param name="ip_server"> - IP адрес сервера в формате "хххх.хххх.хххх.хххх" </param>
        /// <param name="port_server"> - номер порта сервера </param>
        /// <param name="logger"> - объект логгирования </param>
        TCP_socketClient_t(std::string ip_server, unsigned short port_server, log_t& logger);

        /// <summary>
        /// Коструктор с 2 параметрами
        /// </summary>
        /// <param name="serverSockInfo"> - информация о сервере </param>
        /// <param name="logger"> - объект логгирования </param>
        TCP_socketClient_t(sockInfo_t serverSockInfo, log_t& logger);

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
        int Recive(std::string& str_bufer, const std::string str_EndOfMessege = "", const size_t sizeMsg = 0);

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
        int Send(const std::string& str_bufer, const unsigned offset = 0);

        /// <summary>
        /// Метод подключения сокета к серверному сокету
        /// </summary>
        /// <returns> 1 - сокет подключен </returns>
        bool Connected();

        /// <summary>
        /// метод возврата состояния соединения
        /// </summary>
        /// <returns> 1 - соединение есть </returns>
        bool GetConnected() const;

        void ResetConnected();

        void Shutdown();
    protected:
        bool b_connected; // признак подключения сокета к серверу
        sockInfo_t serverInfo; // информация о сервере
    };

    /// <summary>
    /// TCP серверный сокет
    /// </summary>
    class TCP_socketServer_t : public socket_t
    {
    public:
        /// <summary>
        /// конструктор с 3-я параметрами
        /// </summary>
        /// <param name="ip"> - IP адресс в формате "хххх.хххх.хххх.хххх" </param>
        /// <param name="port"> - номер порта </param>
        /// <param name="logger"> - объект логгирования </param>
        TCP_socketServer_t(std::string ip, unsigned short port, log_t& logger);

        /// <summary>
        /// конструктор с 2-я параметрами
        /// </summary>
        /// <param name="sockInfo"> - информация о сокете </param>
        /// <param name="logger"> - объект логгирования </param>
        TCP_socketServer_t(sockInfo_t sockInfo, log_t& logger);

        /// <summary>
        /// метод добавления подключенных клиентов
        /// </summary>
        /// <param name="client"> - ссылка на клиента под замену на подключенного клиента </param>
        /// <returns> 0 - добавлен подключенный клиент,
        ///          -1 - системная ошибка,
        ///          -2 - нет клиентов в очереди на подключение (неблокирующий сокет)</returns>
        int AddClient(TCP_socketClient_t& client);
    };

    /// <summary>
    /// UDP сокет
    /// </summary>
    class UDP_socket_t : public socket_t
    {
    private:
        /// <summary>
        /// метод установки MTU
        /// </summary>
        void setMTU();
    public:

        /// <summary>
        /// метод переноса уникального ресурса (дескпритор) между сокетами
        /// </summary>
        /// <param name="source"> - ссылка на сокет источник - после вызова метода не содержит валидный сокет </param>
        void Move(UDP_socket_t& source);

        /// <summary>
        /// Конструткор с 1 параметром
        /// </summary>
        /// <param name="logger"> - объект для логгирования </param>
        UDP_socket_t(log_t& logger);

        /// <summary>
        /// конструктор с 3-я параметрами
        /// </summary>
        /// <param name="ip"> - IP адресс в формате "хххх.хххх.хххх.хххх" </param>
        /// <param name="port"> - номер порта </param>
        /// <param name="logger"> - объект логгирования </param>
        UDP_socket_t(std::string ip, unsigned short port, log_t& logger);

        /// <summary>
        /// конструктор с 2-я параметрами
        /// </summary>
        /// <param name="sockInfo"> - информация о сокете </param>
        /// <param name="logger"> - объект логгирования </param>
        UDP_socket_t(sockInfo_t& sockInfo, log_t& logger);

        /// <summary>
        /// Метод отправки данных в сеть без предварительного соединения
        /// </summary>
        /// <param name="buffer"> - область с данными для отправки </param>
        /// <param name="target"> - информация о сокете приемнике </param>
        /// <returns> 0 - отправлено все сообщение;
        ///         N>0 - отправлено N байт;
        ///          -1 - системная ошибка;
        ///          -2 - размер сообщения больше MTU или сокет не валидный;
        ///          -3 - сокет не готов к отправке (неблокирующий сокет) </returns>
        int SendTo(const std::string& buffer, sockInfo_t target);

        /// <summary>
        /// Метод отправки данных в сеть без предварительного соединения, отправка предыдущему сокету с которым было взаимодействие
        /// </summary>
        /// <param name="buffer"> - область с данными для отправки </param>
        /// <returns> 0 - отправлено все сообщение;
        ///         N>0 - отправлено N байт;
        ///          -1 - системная ошибка;
        ///          -2 - размер сообщения больше MTU или сокет не валидный;
        ///          -3 - сокет не готов к отправке (неблокирующий сокет) </returns>
        int SendTo(const std::string& buffer);

        /// <summary>
        /// Метод отправки данных в сеть без предварительного соединения
        /// </summary>
        /// <param name="buffer"> - область с данными для отправки </param>
        /// <param name="ip"> - IP адресс в формате "хххх.хххх.хххх.хххх" </param>
        /// <param name="port"> - номер порта </param>
        /// <returns> 0 - отправлено все сообщение;
        ///         N>0 - отправлено N байт;
        ///          -1 - системная ошибка;
        ///          -2 - размер сообщения больше MTU или сокет не валидный;
        ///          -3 - сокет не готов к отправке (неблокирующий сокет) </returns>
        int SendTo(const std::string& buffer, std::string ip, unsigned short port);

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
        int RecvFrom(std::string& buffer, const std::string str_EndOfMessege = "", const size_t sizeMsg = 0);

        /// <summary>
        /// метод возврата информации о сокете с которым происходило последнее взаимодействие (отправка/прием данных)
        /// </summary>
        /// <returns> сокет с которым происходило последнее взаимодействие (отправка/прием данных) </returns>
        sockInfo_t GetLastCommunication() const;

        /// <summary>
        /// метод возврата MTU
        /// </summary>
        /// <returns> максимальный размер сообщения </returns>
        unsigned int MTU() const;
    private:
        sockInfo_t lastCommunicationSocket; // Последний сокет, с кем происходило взаимодействие
        unsigned int u32_MTU; // максимальный размер отправляемых данных
    };

    /// <summary>
    /// Класс мультиплексирования неблокирующих сокетов. 
    /// При удалении сокета раньше менеджера, необходимо удалить сокет из списков менеджера
    /// </summary>
    class NonBlockSocket_manager_t : private RAII_OSsock
    {
    protected:
        /// <summary>
        /// метод добавления сокета в один из списков
        /// </summary>
        /// <param name="m_sock"> - ассоциативный массив для добавления </param>
        /// <param name="socket"> - сокет на добавление </param>
        /// <returns> 1 - сокет добавлен </returns>
        bool addSocket(std::unordered_map<int, std::weak_ptr<socket_t>>& m_sock, const std::weak_ptr<socket_t>& socket);

        /// <summary>
        /// Метод удаления сокета из списков
        /// </summary>
        /// <param name="m_sock"> - ассоциативный массив для добавления </param>
        /// <param name="socket"> - сокет на удаление </param>
        /// <returns> 1 - сокет удален </returns>
        bool deleteSocket(std::unordered_map<int, std::weak_ptr<socket_t>>& m_sock, const std::weak_ptr<socket_t>& socket);

        /// <summary>
        /// Метод обновления структуры pollfd
        /// </summary>
        void UpdatePollfd();

        /// <summary>
        /// функция мультиплексированя неблокирующих сокетов
        /// </summary>
        /// <param name="timeOut"> - время timeout для функции мультиплексирования </param>
        /// <returns> -1 - системная ошибка; 0 - вышел таймаут и событий не произошло; N>0 - кол-во событий </returns>
        int Poll(int timeOut);
    public:
        /// <summary>
        /// Конструктор с одним параметром
        /// </summary>
        /// <param name="logger"> - объект для логиирования </param>
        NonBlockSocket_manager_t(log_t& logger);

        /// <summary>
        /// Конструктор с двумя параметрами
        /// </summary>
        /// <param name="size"> - предварительное количество наблюдаемых сокетов </param>
        /// <param name="logger"> - объект для логирования </param>
        NonBlockSocket_manager_t(int size, log_t& logger);

        /// <summary>
        /// метод добавления отправителя
        /// </summary>
        /// <param name="socket"> - целевой сокет </param>
        /// <returns> 1 - сокет добавлен </returns>
        bool AddSender(const std::weak_ptr<socket_t>& socket);

        /// <summary>
        /// метод удаления отправителя
        /// </summary>
        /// <param name="socket"> - целевой сокет </param>
        /// <returns> 1 - сокет удален </returns>
        bool deleteSender(const std::weak_ptr<socket_t>& socket);

        /// <summary>
        /// метод добавления читателя
        /// </summary>
        /// <param name="socket"> - целевой сокет </param>
        /// <returns> 1 - сокет добавлен </returns>
        bool AddReader(const std::weak_ptr<socket_t>& socket);

        /// <summary>
        /// метод удаления читателя
        /// </summary>
        /// <param name="socket"> - целевой сокет </param>
        /// <returns> 1 - сокет удален </returns>
        bool deleteReader(const std::weak_ptr<socket_t>& socket);

        /// <summary>
        /// метод добавления сервера
        /// </summary>
        /// <param name="socket"> - целевой сокет </param>
        /// <returns> 1 - сокет добавлен </returns>
        bool AddServer(const std::weak_ptr<socket_t>& socket);

        /// <summary>
        /// метод удаления сервера
        /// </summary>
        /// <param name="socket"> - целевой сокет </param>
        /// <returns> 1 - сокет удален </returns>
        bool deleteServer(const std::weak_ptr<socket_t>& socket);

        /// <summary>
        /// метод добавления клиента
        /// </summary>
        /// <param name="socket"> - целевой сокет </param>
        /// <returns> 1 - сокет добавлен </returns>
        bool AddClient(const std::weak_ptr<socket_t>& socket);

        /// <summary>
        /// метод удаления клиента
        /// </summary>
        /// <param name="socket"> - целевой сокет </param>
        /// <returns> 1 - сокет удален </returns>
        bool deleteClient(const std::weak_ptr<socket_t>& socket);

        /// <summary>
        /// метод проверки готовности отправителя к отправке
        /// </summary>
        /// <param name="socket"> - целевой сокет </param>
        /// <returns> 1 - сокет готов </returns>
        bool GetReadySender(const std::weak_ptr<socket_t>& socket) const;

        /// <summary>
        /// метод проверки готовности читателя к приему
        /// </summary>
        /// <param name="socket"> - целевой сокет </param>
        /// <returns> 1 - сокет готов </returns>
        bool GetReadyReader(const std::weak_ptr<socket_t>& socket) const;

        /// <summary>
        /// метод проверки наличия запроса на подключения у сервера
        /// </summary>
        /// <param name="socket"> - целевой сокет </param>
        /// <returns> 1 - есть подключение </returns>
        bool GetReadyServer(const std::weak_ptr<socket_t>& socket) const;

        /// <summary>
        /// метод проверки подтверждения подключения к серверу у клиента
        /// </summary>
        /// <param name="socket"> - целевой сокет </param>
        /// <returns> 1 - есть подключение </returns>
        bool GetReadyClient(const std::weak_ptr<socket_t>& socket) const;

        /// <summary>
        /// основной метод работы мультиплесора
        /// </summary>
        /// <param name="timeOut"> - время ожидания мультиплесора </param>
        /// <returns> 1 - принято хотя бы одно событие </returns>
        bool Work(const int timeOut);
    protected:
        std::vector <struct pollfd> v_fds; // динамический массив структур pollfd
        std::unordered_map<int, std::weak_ptr<socket_t>> m_senderSocket; // мап сокетов ожидающих отправку
        std::unordered_map<int, std::weak_ptr<socket_t>> m_readerSocket; // мап сокетов ожидающих прием
        std::unordered_map<int, std::weak_ptr<socket_t>> m_serverSocket; // мап сокетов ожидающих входящее подключение
        std::unordered_map<int, std::weak_ptr<socket_t>> m_clientSocket; // мап сокетов ожидающих изходящее подключение
        std::unordered_map<int, std::weak_ptr<socket_t>> m_readySender; // мап готовых отправителей
        std::unordered_map<int, std::weak_ptr<socket_t>> m_readyReader; // мап готовых читателей
        std::unordered_map<int, std::weak_ptr<socket_t>> m_readyServer; // мап готовых серверов
        std::unordered_map<int, std::weak_ptr<socket_t>> m_readyClient; // мап готовых клиентов
        bool b_change; // флаг изменения структур pollfd
        log_t& logger; // объект логгирования
    };
};






#endif /* NETWORK_H_ */
