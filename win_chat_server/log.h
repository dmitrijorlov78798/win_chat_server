#pragma once
#ifndef LOG_T_H_
#define LOG_T_H_

#include <iostream>
#include <fstream>
#include <string>

#ifdef DEBUG
#define DEBUG_TRACE(logger, string) logger.doDebugTrace(string)
#else
#define DEBUG_TRACE(logger, string)
#endif

/// <summary>
/// Класс для логгирования событий через файл и/или консоль
/// </summary>
class log_t
{
public:
    log_t();
    log_t(std::string nameLogFile, bool consoleActive);
    std::string getTime();
    void doLog(std::string log, int errCode = 0x80000000);
#ifdef DEBUG
    void doDebugTrace(std::string trace);
#endif
    int GetLastErr() const;
    virtual ~log_t();
protected:
    std::ofstream logFile; // файл для логгирования
    bool consoleActive; // флаг вывода в консоль
    int time_zone; // часовой пояс
    int lastErr; // код последней ошибки
};

#endif // !LOG_T
