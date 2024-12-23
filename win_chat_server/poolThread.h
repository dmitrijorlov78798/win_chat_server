#pragma once
#ifndef POOLTHREAD_H_
#define POOLTHREAD_H_

#include <thread>
#include <atomic>
#include <map>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <memory>

typedef unsigned long long taskID; // номер задачи

/// <summary>
/// Абстрактный класс пользовательской задачи
/// </summary>
class ABStask
{
public:
    /// <summary>
    /// Основной метод работы
    /// </summary>
    /// <param name="stop"> - при получении данного признака, необходимо завершить работу метода </param>
    virtual void Work(const volatile std::atomic_bool& stop) = 0;
    virtual ~ABStask() {}
};

/// <summary>
/// класс запуска пользовательской задачи
/// </summary>
class task_t
{
    friend struct thread_t;
    friend class poolThread_t;
protected :
    /// <summary>
    /// конструктор
    /// </summary>
    /// <param name="cv_wakeUp"> - условная переменная оповещения внешнего окружения о завершении пользовательской задачи </param>
    /// <param name="mtx_wakeUp"> - мьютекс к условной переменной </param>
    task_t(std::condition_variable& cv_wakeUp, std::mutex& mtx_wakeUp);

    /// <summary>
    /// Метод основной работы. Запускается в другом потоке
    /// </summary>
    void Work();

    /// <summary>
    /// Метод обновления задачи
    /// </summary>
    /// <param name="userTask"> - указатель на объект пользовательской таски </param>
    /// <returns> 1 - таска обновлена </returns>
    bool UpdateTask(std::shared_ptr<ABStask> userTask);

    /// <summary>
    /// Метод проверки активности
    /// </summary>
    /// <returns> 1 - таска активна </returns>
    bool GetActive() const;

    /// <summary>
    /// Метод окончательного выхода из таски
    /// </summary>
    void Stop();

    virtual ~task_t();
protected:
    std::condition_variable cv_condition; // условная переменная для начала работы обновленной пользовательской задачи
    mutable std::mutex mutex;// мьютекс для условной переменной 
    std::condition_variable& cv_wakeUp; //  условная переменная оповещения внешнего окружения о завершении пользовательской задачи
    std::mutex& mtx_wakeUp; // мьютекс для условной переменной внешнего окружения
    volatile std::atomic_bool b_stop; // флаг останова данной таски
    volatile std::atomic_bool b_active; // флаг активности данной таски

    std::shared_ptr<ABStask> p_userTask; // указатель на пользовательскую задачу
};

/// <summary>
/// Структура реализующая взаимодействие потока с таской
/// </summary>
struct thread_t
{
    friend class poolThread_t;
 
    /// <summary>
    /// конструктор по умолчанию
    /// </summary>
    thread_t(std::condition_variable& cv_wakeUp, std::mutex& mtx_wakeUp);
    ///деструктор
    virtual ~thread_t();

protected :
    std::thread thread; // стандартный поток
    task_t task; // таска
};


/// <summary>
/// Класс пулл потоков
/// </summary>
class poolThread_t
{
    friend class poolThread_manager_t;
protected :
    /// <summary>
    /// конструктор
    /// </summary>
    /// <param name="cv_wakeUp"> - условная переменная внешнего окружения - сигнал о выподнении задачи </param>
    /// <param name="mtx_wakeUp"> - мьютексная защита переменной </param>
    poolThread_t(std::condition_variable& cv_wakeUp, std::mutex& mtx_wakeUp, const size_t SIZE); 

    virtual ~poolThread_t()
    {}

    /// <summary>
    /// Мето обновления таски
    /// </summary>
    /// <param name="userTask"> - указатель на объект пользовательской таски </param>
    /// <returns> INVALID_TASK - нет свободных потоков; ID>INVALID_TASK - ID таски </returns>
    bool UpdateTask(std::shared_ptr<ABStask> userTask, taskID ID);
    
    /// <summary>
    /// Метод проверки активности таски
    /// </summary>
    /// <param name="ID"> - ID целевой таски </param>
    /// <returns> 1 - таска активна </returns>
    bool GetActiveTask(taskID ID);

    /// <summary>
    /// метод опроса наличия свободного потока
    /// </summary>
    /// <returns> 1 - есть хотя бы один свободный поток </returns>
    bool GetFree() const;

protected:
    std::vector<std::shared_ptr<thread_t>> v_thread; // вектор потоков
    std::vector<taskID> v_taskID; // вектор ID тасков
    const size_t SIZE; // размер пула
    mutable bool free; // флаг наличия свободного потока
};

// статусы переданой в пул задачи
#define NON_DEFINE 0 // не определено(невалидный taskID)
#define ACTIVE 1 // задача в процессе выполнения 
#define COMPLECTED 2 // задача выполнена
#define EXCEPTION 3 // задача в очереди на выполнение 

/// <summary>
/// вспомогательная структура обертка на пользовательской задачей
/// </summary>
struct status_p_task_t
{
    friend class poolThread_manager_t;
    status_p_task_t() : status(0), p_task(nullptr)
    {}
protected :
    char status; // статус задачи
    std::shared_ptr<ABStask> p_task; // указатель на задачу
};

/// <summary>
/// Менеджер пула потоков, принимает пользовательские задачи и передает их в пул
/// </summary>
class poolThread_manager_t
{
protected:
    
    /// <summary>
    /// метод работы управляющего потока
    /// </summary>
    void Update();
   
public:
    /// <summary>
    /// конструктор по умолчанию
    /// </summary>
    poolThread_manager_t(const size_t SIZE);

    /// <summary>
    /// деструктор
    /// </summary>
    virtual ~poolThread_manager_t();

    /// <summary>
    /// метод добавления новой задачи
    /// </summary>
    /// <param name="p_task"> - smart_ptr на пользовательскую задачу </param>
    /// <returns> присвоенный пользовательской задаче уникальный номер (дескриптор) </returns>
    taskID AddTask(std::shared_ptr<ABStask> p_task);

    /// <summary>
    /// метод получения статуса задачи
    /// </summary>
    /// <param name="ID"></param>
    /// <returns> NON_DEFINE (0) - не определено(невалидный taskID)
    ///           ACTIVE (1) - задача в процессе выполнения 
    ///           COMPLECTED (2) - задача выполнена
    ///           EXCEPTION (3) - задача в очереди на выполнение  </returns>
    int GetStatusTask(taskID ID);

protected :
    poolThread_t slave_pool; // управляемый пул потоков
    taskID counter; // автоинкремент
    std::map<taskID, status_p_task_t> m_task; // мап пользовательских выполняемых задач и задач в очереди на выполнение
    std::thread master_thread; // управляющий поток, по мере необходимости, подгружающий задачи в пул
    std::condition_variable cv_condition; // условная переменная, толкаемая по завершении одной из задач
    mutable std::mutex mutex;// мьютекс - защита распределенных данных
    volatile std::atomic_bool stop; // флаг остановки всех процессов
};



#endif /* POOLTHREAD_H_ */

