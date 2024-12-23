
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
    linkOff, // собеседники потеряли связь
    printinfo // вывод информации по соединению
};

/// <summary>
/// класс декоратор над std::string для хранения сообщения, состоящего из 
/// заголовка (тип сообщения)-6 символов + текст + конец сообщения(EOM)-5 символов
/// </summary>
class msg_t
{
public :
    /// <summary>
    /// контсруктор
    /// </summary>
    /// <param name="type"> -- тип сообщения</param>
    /// <param name="text"> -- текс сообщения</param>
    msg_t(TypeMsg type = TypeMsg::defaul, std::string text = "")
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
            this->text = "[LNK1][EOM]";
            break;
        case linkOff:
            this->text = "[LNK0][EOM]";
            break;
        case printinfo:
            this->text = "[INFO]"; // ЕОМ не нужен, для внутреннего пользованя клиента
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
            else if (header == "[LNK1]")
                result = TypeMsg::linkOn;
            else if (header == "[LNK0]")
                result = TypeMsg::linkOff;
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
    void Print(std::string buf) const
    {
        buf.clear();
        if (Type() == TypeMsg::normal) // полезная нагрузка только в нормальном сообщении
            buf.assign(text, 6, text.size() - 11); // выдаем текст без заголовка и конца сообщения
    }
protected :
    std::string text; // строка хранящее сообщение, согласно формату, опраделенному выше
};

/// <summary>
/// Класс по обработке клиентского соединения в отдельном потоке (пуле потоков).
/// реализован на синхронных сокетах (предполагается, что ацептор также синхронен) 
/// </summary>
class session_t : public ABStask, public network::TCP_socketClient_t
{
public :
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
         network::TCP_socketClient_t(logger), l_visavi(l_visavi), mutex(mutex), acceptor(acceptor), b_shutDown(b_shutDown)
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
    {   // пока нет остановки и есть связь, и мы приняли сообщение, и нет отключения сервера
        while (!stop && b_connected && 0 == Recive(msg_RX.Update(), msg_RX.EOM()) && !b_shutDown) 
        {   // блокируем мьютекс списка собеседников, 
            std::lock_guard<std::mutex> lock(mutex);
            std::cout << "IN: " << msg_RX.Str() << '\n'; ////////////////////////////////наладка
            // обновляем флаги
            b_connected &= GetConnected() && !(msg_RX.Type() == TypeMsg::Exit);
            b_shutDown = b_shutDown ||  msg_RX.Type() == TypeMsg::shutDown;

            // идем по списку собеседников
            for (auto it = l_visavi.begin(); it != l_visavi.end(); )
                if (auto ptr = it->lock()) // если указатель валидный
                {
                    if (ptr->getSocket() != this->getSocket()) // себе не отправляем
                        ptr->Send(msg_RX.Str()); // отправляем сообщение собеседнику
                    ++it;
                }
                else
                    it = l_visavi.erase(it); // если указатель нулевой - удаляем

            // если собеседников нет
            if (l_visavi.empty())
            {   // диагностируем
                msg_t msgTmp(TypeMsg::linkOff);
                Send(msgTmp.Str());
            }

            // если сервер отключается из-за нас
            if (b_shutDown && msg_RX.Type() == TypeMsg::shutDown)            
            {
                std::cout << "shut down\n"; ////////////////////////////////наладка
                network::TCP_socketClient_t signal(acceptor, logger); // толкаем ацептор в главном потоке
                return; // выходим
            }
        }
    }
protected :
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
public :
    /// <summary>
    /// конструктор
    /// </summary>
    /// <param name="port"> -- номер порта для прослушивания </param>
    chat_manager_t(unsigned port) : logger("server.log", true), acceptor(IP_ADRES, port, logger), tmpClient(logger), pool(MAX_COUNT_CLIENT)
    {}
    /// <summary>
    /// основной метод работы
    /// </summary>
    void Work()
    {
        while (0 == acceptor.AddClient(tmpClient) && !b_shutDown)
        {   // идем по списку задач(собеседников) и пробуем захватить их (либо удалить, если указатели уже не валидны)
            if (l_task.size() < MAX_COUNT_CLIENT) // если размер позволяет
            {
                std::lock_guard<std::mutex> lock(mutex);
                auto newTask = std::make_shared<session_t>(l_task, mutex, tmpClient, acceptor.GetSockInfo(), b_shutDown, logger);
                pool.AddTask(newTask);
                l_task.push_back(newTask);
            }
            else
            {
                msg_t msg(TypeMsg::normal, "Maximum number of clients reached");
                tmpClient.Send(msg.Str());
            } 
        }
    }
protected :
    log_t logger; // объект для логгирования
    network::TCP_socketServer_t acceptor; // ацептор
    network::TCP_socketClient_t tmpClient; // буфер для получения клиентов от ацептора
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
    printf("run_server\n");
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
        b_result = r_port != 0 && r_port != ULONG_MAX;
    }

    return b_result;
}

