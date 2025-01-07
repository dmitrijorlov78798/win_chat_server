
#include <iostream>
#include <string>
#include <mutex>
#include <list>
#include <condition_variable>

#include "network.h"
#include "poolThread.h"

#define IP_ADRES "127.0.0.1"
#define MAX_COUNT_CLIENT 2

/// <summary>
/// тип сообщений
/// </summary>
enum TypeMsg
{
    defaul, // неопознанное
    normal, // нормальное, с полезной нагрузкой
    // сервисные:
    Exit, // отключение клиента
    shutDown, // отключение сервера
    linkOn, // собеседники на связи
    printinfo // вывод информации по соединению
};

/// <summary>
/// класс декоратор над std::string для хранения сообщения, состоящего из 
/// заголовка (тип сообщения)-6 символов + текст + конец сообщения(EOM)-5 символов
/// </summary>
class msg_t
{
public:
    /// <summary>
    /// контсруктор
    /// </summary>
    /// <param name="type"> -- тип сообщения</param>
    /// <param name="text"> -- текст сообщения</param>
    msg_t(TypeMsg type = TypeMsg::defaul, std::string text = "") : offset(0)
    {
        switch (type)
        {
        case normal:
            this->text = "[NORM]" + text + "[EOM]"; // полезная нагрузка лишь здесь
            break;
        case Exit:
            this->text = "[EXIT][EOM]";
            break;
        case shutDown:
            this->text = "[SHUT][EOM]";
            break;
        case linkOn:
            this->text = "[LINK][EOM]";
            break;
        case printinfo:
            this->text = "[INFO][EOM]"; 
            break;
        default:
            break;
        }
    }

    /// <summary>
    /// метод получения не константной ссылки на внутренний std::string с сообщением
    /// </summary>
    /// <returns> не константная ссылка на внутренний std::string с сообщением </returns>
    std::string& Update()
    {
        offset = 0;
        return text;
    }

    /// <summary>
    /// метод получения константной ссылки на внутренний std::string с сообщением 
    /// </summary>
    /// <returns> константная ссылка на внутренний std::string с сообщением </returns>
    const std::string& Str() const
    {
        return text;
    }

    /// <summary>
    /// метод получения типа сообщения
    /// </summary>
    /// <returns> типа сообщения </returns>
    TypeMsg Type() const
    {
        TypeMsg result = TypeMsg::defaul;

        if (text.size() >= 6)
        {
            std::string header(text, 0, 6); // извлекаем заголовок
            if (header == "[NORM]")
                result = TypeMsg::normal;
            else if (header == "[EXIT]")
                result = TypeMsg::Exit;
            else if (header == "[SHUT]")
                result = TypeMsg::shutDown;
            else if (header == "[LINK]")
                result = TypeMsg::linkOn;
            else if (header == "[INFO]")
                result = TypeMsg::printinfo;
        }

        return result;
    }

    /// <summary>
    /// метод получения конца сообщения
    /// </summary>
    /// <returns> конец сообщения </returns>
    std::string EOM() const
    {
        return "[EOM]";
    }

    /// <summary>
    /// метод получения текста сообщения без заголовка и конца сообщения
    /// </summary>
    /// <param name="buf"> -- ссылка на буфер, куда будет положен полезный текст сообщения </param>
    ///  <returns> 1 -- текст получен </returns>
    bool Print(std::string& buf) const
    {
        buf.clear();
        switch (Type())
        { // расшифровка сервесных сообщений
        case TypeMsg::shutDown:
            buf = "server get command shutdown";
            break;
        case TypeMsg::Exit:
            buf = "server get command exit from visavi client";
            break;
        case TypeMsg::printinfo:
            buf = "other visavi not connected";
            break;
        case TypeMsg::linkOn:
            buf = "server get connected from visavi";
            break;
        case TypeMsg::defaul:
            buf = "server recived defined message";
            break;
        case TypeMsg::normal:
            buf.assign(text, 6, text.size() - 11); // выдаем текст без заголовка и конца сообщения
            break;
        default:
            break;
        }

        return !text.empty();
    }

    /// <summary>
    /// метод задания смещения
    /// </summary>
    /// <param name="offset"> -- смещение </param>
    void SetOffset(unsigned offset)
    {
        if (offset < text.size())
            this->offset = offset;
    }

    /// <summary>
    /// метод возврата смещения
    /// </summary>
    /// <returns> -- смещение </returns>
    unsigned GetOffset() const
    {
        return offset;
    }

protected:
    std::string text; // строка хранящее сообщение, согласно формату, опраделенному выше
    unsigned offset; // смещение от начала сообщения
};

/// <summary>
/// Класс по обработке клиентского соединения в отдельном потоке (пуле потоков).
/// реализован на синхронных сокетах (предполагается, что ацептор также синхронен) 
/// </summary>
class session_t : public ABStask, public network::TCP_socketClient_t
{
public:
    /// <summary>
    /// конструктор
    /// </summary>
    /// <param name="l_visavi"> -- ссылка на список собеседников </param>
    /// <param name="mutex"> -- ссылка на мьютекс, защищаюйщий список собеседников </param>
    /// <param name="client"> -- ссылка на клиентский сокет, полученный ацептором </param>
    /// <param name="aceptor"> -- информация об ацепторе </param>
    /// <param name="b_shutDown"> -- ссылка на флаг отключения сервера </param>
    /// <param name="logger"> -- ссылка на обект логгирования </param>
    session_t(std::list<std::weak_ptr<session_t>>& l_visavi,
        std::mutex& mutex, network::TCP_socketClient_t& client,
        network::sockInfo_t acceptor,
        volatile std::atomic_bool& b_shutDown,
        log_t& logger) :
        network::TCP_socketClient_t(logger), l_visavi(l_visavi), mutex(mutex), msg_RX(TypeMsg::linkOn), acceptor(acceptor), b_shutDown(b_shutDown)
    {
        Move(client); // кастомная (самодельная) move семантика
        b_connected = GetConnected();
    }

    // деструктор
    ~session_t()
    {}

    /// <summary>
    /// потоковый метод работы, запускается в отдельном потоке в пуле потоков 
    /// </summary>
    /// <param name="stop"> -- флаг остановки задачи от пула потоков </param>
    void Work(const volatile std::atomic_bool& stop) override
    {
        bool b_firstIter = true; // флаг первой итерации цикла
        do // начинаем с рукопожатия
        {   // блокируем мьютекс списка собеседников,
            std::lock_guard<std::mutex> lock(mutex);
            //std::cout << "IN: " << msg_RX.Str() << '\n'; ////////////////////////////////наладка
            // обновляем флаги
            b_connected &= GetConnected() && msg_RX.Type() != TypeMsg::Exit;
            b_shutDown = b_shutDown || msg_RX.Type() == TypeMsg::shutDown;

            // идем по списку собеседников
            for (auto it = l_visavi.begin(); it != l_visavi.end(); )
                if (auto ptr = it->lock()) // если указатель валидный
                {
                    if (ptr->getSocket() != this->getSocket()) // себе не отправляем
                        if (0 != ptr->Send(msg_RX.Str())) // отправляем сообщение собеседнику
                        { // диагностика ошибки
                            msg_t msgTmp(TypeMsg::normal, "error send message visavi, errno: " + std::to_string(logger.GetLastErr()));
                            Send(msgTmp.Str());
                        }

                    ++it;
                }
                else
                    it = l_visavi.erase(it); // если указатель нулевой - удаляем

            // если в беседе только мы и мы пытаемся написать другим 
            if (l_visavi.size() == 1 && msg_RX.Type() == TypeMsg::normal)
            {   // диагностируем
                msg_t msgTmp(TypeMsg::printinfo);
                Send(msgTmp.Str());
            }
            else if (b_firstIter) // в первый цикл, считаем собеседников
                for (size_t indx = l_visavi.size(); indx > 1; --indx)
                {
                    msg_t msgTmp(TypeMsg::linkOn);
                    Send(msgTmp.Str());
                }
            b_firstIter = false;

            if(!b_connected)
                logger.doLog("Close client, count client: " + std::to_string(l_visavi.size() - 1));

            // если сервер отключается из-за нас
            if (b_shutDown && msg_RX.Type() == TypeMsg::shutDown)
            {
                msg_t msgTmp(TypeMsg::normal, "server shutdown"); // подтверждаем клиенту свое отключение
                Send(msgTmp.Str());
                network::TCP_socketClient_t signal(acceptor, logger); // толкаем ацептор в главном потоке
                logger.doLog("Close client, count client: " + std::to_string(l_visavi.size() - 1));
                return; // выходим
            }
            // пока нет остановки и есть связь, и мы приняли сообщение, и нет отключения сервера
        } while (!stop && b_connected && 0 == Recive(msg_RX.Update(), msg_RX.EOM()) && !b_shutDown);
    }
protected:
    std::list<std::weak_ptr<session_t>>& l_visavi; // ссылка на список собеседников
    std::mutex& mutex; // ссылка на мьютекс, защищающий список собеседников
    msg_t msg_RX; // буфер для принятого от клиента сообщения
    network::sockInfo_t acceptor; // информация об ацепторе
    volatile std::atomic_bool& b_shutDown; // ссылка на флаг отключения сервера
    bool b_connected; // флаг наличия соединения с клиентом
};

/// <summary>
/// класс управляющий чатом
/// </summary>
class chat_manager_t
{
public:
    /// <summary>
    /// конструктор
    /// </summary>
    /// <param name="port"> -- номер порта для прослушивания </param>
    chat_manager_t(unsigned port) : logger("server.log", true), acceptor(IP_ADRES, port, logger), pool(MAX_COUNT_CLIENT)
    {
        logger.doLog("server run");
    }

    ~chat_manager_t()
    { 
        std::chrono::seconds sec{ 1 }; // ждем секунду на завершение потоков
        std::this_thread::sleep_for(sec);
        // собеседники должны успеть толкнуть свои соединения, для завершения задач(соединений)
        std::lock_guard<std::mutex> lock(mutex); // проверяем это
        // идем по списку задач(собеседников) и чистим выполненные
        for (auto it = l_task.begin(); it != l_task.end(); )
        {
            if (auto ptr = it->lock()) // выключаем живые соединения, если еще кто жив
                ptr->Shutdown();
            it = l_task.erase(it);
        }
        logger.doLog("server shutdown");
    }
    /// <summary>
    /// основной метод работы
    /// </summary>
    void Work()
    {
        while (!b_shutDown)
        {
            network::TCP_socketClient_t tmpClient(logger); // буфер для получения клиентов от ацептора
            if (0 == acceptor.AddClient(tmpClient)) // получаем подключение
            { // если получили валидное подключение
                std::lock_guard<std::mutex> lock(mutex);
                if (!b_shutDown) // и нет команды на отключение
                {
                    // идем по списку задач(собеседников) и чистим выполненные
                    for (auto it = l_task.begin(); it != l_task.end(); )
                        if (auto ptr = it->lock())
                            ++it;
                        else
                            it = l_task.erase(it);

                    if (l_task.size() < MAX_COUNT_CLIENT) // если размер позволяет
                    {   // добавляем задачу (собеседника)
                        auto newTask = std::make_shared<session_t>(l_task, mutex, tmpClient, acceptor.GetSockInfo(), b_shutDown, logger);
                        pool.AddTask(newTask);
                        l_task.push_back(newTask);
                        logger.doLog("Connected new client, count client: " + std::to_string(l_task.size()));
                    }
                    else
                    { // иначе, диагностируем превышение размера
                        msg_t msg(TypeMsg::normal, "Maximum number of clients reached");
                        tmpClient.Send(msg.Str());
                        logger.doLog("Maximum number of clients reached");
                    }
                }
            }
            else
                b_shutDown = true;
        }
    }
protected:
    log_t logger; // объект для логгирования
    network::TCP_socketServer_t acceptor; // ацептор
    poolThread_manager_t pool; // пул потоков
    volatile std::atomic_bool b_shutDown; // флаг отключения сервера
    std::list<std::weak_ptr<session_t>> l_task; // список собеседников
    std::mutex mutex; // мьютекс защиты списка собеседников
};



/// <summary>
/// функция разобра параметров командной строки
/// </summary>
/// <param name="argc"> - количество параметров </param>
/// <param name="argv"> - массив параметров </param>
/// <param name="r_port"> - ссылка на порт для прослушки </param>
/// <returns> 1 - праметры распознаны </returns>
bool parseParam(int argc, char* argv[], unsigned& r_port);


int main(int argc, char* argv[])
{
    unsigned u32_port = 0;

    if (parseParam(argc, argv, u32_port))
    {
        chat_manager_t chat(u32_port);
        chat.Work();
    }
    else
        printf("Invalid parametr's. Please enter the number_port\n");

    return EXIT_SUCCESS;
}

/// <summary>
/// функция разобра параметров командной строки
/// </summary>
/// <param name="argc"> - количество параметров </param>
/// <param name="argv"> - массив параметров </param>
/// <param name="r_port"> - ссылка на порт для прослушки </param>
/// <returns> 1 - праметры распознаны </returns>
bool parseParam(int argc, char* argv[], unsigned& r_port)
{
    bool b_result = false;

    if (argc == 2)
    {
        r_port = std::strtoul(argv[1], NULL, 10);
        b_result = r_port != 0 && r_port != 0xFFFFFFFFUL;
    }

    return b_result;
}

