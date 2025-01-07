#include "log.h"
#include <chrono>
#include <sstream>
#include <iomanip>

/// <summary>
/// Конструктор по умолчанию
/// Если не задаем имя файла логгирования, выводим тоьько в консоль
/// </summary>
log_t::log_t() : consoleActive(true), lastErr(0)
{
    time_zone = 3; // TO_DO
}
/// <summary>
/// Конструктор с 3-я параметрами
/// </summary>
/// <param name="nameLogFile"> - имя файла логгирования </param>
/// <param name="consoleActive"> - флаг на вывод в консоль </param>
log_t::log_t(std::string nameLogFile, bool consoleActive) : consoleActive(consoleActive), lastErr(0)
{
    time_zone = 3; // TO_DO
    logFile.open(nameLogFile.c_str(), std::ios::app); // открываем файл логиррования для дозаписи
    if (!logFile)
    {
        if (consoleActive) std::cout << "logFile.open fail";
        else std::cerr << "logFile.open fail";//TODO check
    }
}

log_t::~log_t()
{
    if (logFile.is_open()) // если файл открыт - закрываем
        logFile.close();
}
/// <summary>
/// Метод для записи в лог
/// </summary>
/// <param name="log"> - строка лога </param>
/// <param name="errCode"> - код ошибки (опционально) </param>
void log_t::doLog(std::string log, int errCode)
{
    // готовим итоговое сообщение лога
    std::string msg = getTime();
    msg.append(" :: "); 
    msg.append(log);
    // если есть код ошибки, добавляем его
    if (errCode != 0x80000000)
    {
        lastErr = errCode; // запоминаем значение ошибки
        msg.append(" errno: ");
        msg.append(std::to_string(errCode));
    }
    // вывод в консоль
    if (consoleActive) std::cout << msg << '\n';
    // вывод в файл
    if (logFile.is_open())
    {
        logFile << msg << '\n';
        logFile.flush();
    }
}
#ifdef DEBUG
/// <summary>
/// Метод для записи в лог трейса в режиме отладки
/// </summary>
/// <param name="log"> - строка лога </param>
void log_t::doDebugTrace(std::string trace)
{
    // готовим итоговое сообщение трейса
    std::string msg = getTime();
    msg.append(" :: ");
    msg.append(trace);
    // вывод в консоль
    if (consoleActive) std::cout << trace << '\n';
    // вывод в файл
    if (logFile.is_open()) logFile << trace << '\n';
}
#endif
/// <summary>
/// Метод возврата кода последней ошибки, переданой через метод doLog()
/// </summary>
/// <returns> код последней ошибки, переданой через метод doLog() </returns>
int log_t::GetLastErr() const
{
    return lastErr;
}
/// <summary>
/// метод вывода времени
/// </summary>
/// <returns> строка формата "гггг.мм.дд->чч:мм:сс"</returns>
std::string log_t::getTime()
{
    // С++17 вариант
    std::stringstream result; // результат

    bool leap = false; // флаг весокосного года

    auto now = std::chrono::system_clock::now().time_since_epoch();// 1970 четверг
    auto msec = std::chrono::duration_cast<std::chrono::milliseconds> (now); // миллисекунды
    auto sec = std::chrono::duration_cast<std::chrono::seconds> (now); // секунды
    auto min = std::chrono::duration_cast<std::chrono::minutes> (now); // минуты
    auto tempHour = std::chrono::duration_cast<std::chrono::hours> (now); // часы
    // больше С++17 нам не дает, дальше решаем сами(не актуально в С++20) 
    int hour = tempHour.count() + time_zone; // +3 московский часовой пояс 

    int totalDay = hour / 24; // общее количество дней
    std::string weekDays; // день недели

    switch ((totalDay + 3) % 7) // начинаем с четверга 1970 года
    {
    case 0:
        weekDays = "monday";
        break;
    case 1:
        weekDays = "tuesday";
        break;
    case 2:
        weekDays = "wednesday";
        break;
    case 3:
        weekDays = "thursday";
        break;
    case 4:
        weekDays = "friday";
        break;
    case 5:
        weekDays = "saturday";
        break;
    case 6:
        weekDays = "sunday";
        break;
    default:
        break;
    }

    int year = 1969; // общее количество лет
    int day = 0; // дни

    while (totalDay > 0) // пока общее количество дней еще есть
    {
        ++year; // плюс год
        leap = year % 4 == 0; // год был весокосный?
        day = totalDay; // предварительно устанавливаем дату
        totalDay -= leap ? 366 : 365; // вычитаем год дней из общего количества 
    }

    int mounth = 0; // месяц устанавливаем в  зависимости от количества дней в этом году
    if (day < 31) mounth = 1;
    else if (leap ? day < 59 : day < 60) { mounth = 2; day -= 31; }
    else if (leap ? day < 90 : day < 91) { mounth = 3; day -= leap ? 59 : 60; }
    else if (leap ? day < 120 : day < 121) { mounth = 4; day -= leap ? 90 : 91; }
    else if (leap ? day < 151 : day < 152) { mounth = 5; day -= leap ? 120 : 121; }
    else if (leap ? day < 181 : day < 182) { mounth = 6; day -= leap ? 151 : 152; }
    else if (leap ? day < 212 : day < 213) { mounth = 7; day -= leap ? 181 : 182; }
    else if (leap ? day < 243 : day < 244) { mounth = 8; day -= leap ? 212 : 213; }
    else if (leap ? day < 273 : day < 274) { mounth = 9; day -= leap ? 243 : 244; }
    else if (leap ? day < 304 : day < 305) { mounth = 10; day -= leap ? 273 : 274; }
    else if (leap ? day < 334 : day < 335) { mounth = 11; day -= leap ? 304 : 305; }
    else if (leap ? day < 365 : day < 366) { mounth = 12; day -= leap ? 334 : 335; }
    // готовим результат формата "гггг.мм.дд-день недели-чч:мм:сс:мсмс"
    result << std::setfill('0') << year << '.' << std::setw(2) << mounth << '.' << std::setw(2) << day << '-' << weekDays << '-' //дата
        << std::setw(2) << hour % 24 << ':' << std::setw(2) << min.count() % 60 << ':' << std::setw(2) << sec.count() % 60 //время
        << '.' << std::setw(3) << msec.count() % 1000;//милисекунды

     return std::string(result.str());

    // С++03 - вариант (устаревший, не используется)
    /*char result[64]; // результат
    std::memset(&result, 0, sizeof(result));
    time_t t1 = time(NULL);//возвращающая текущее время в формате time_t — количество секунд, прошедших с 00:00 1 января 1970.
    tm t = *localtime(&t1);//Функция localtime() позволяет перевести time_t в структуру tm, которая состоит из полей, представляющих отдельно часы, минуты, месяц, год и т. д.
    std::sprintf(result ,"%.4d.%.2d.%.2d.->%.2d:%.2d:%.2d", t.tm_year + 1900, t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
    return std::string(result);*/
}

