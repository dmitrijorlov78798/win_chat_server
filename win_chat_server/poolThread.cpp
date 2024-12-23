#include "poolThread.h"

/// <summary>
/// Конструктор по умолчанию
/// </summary>
task_t::task_t(std::condition_variable& cv_wakeUp, std::mutex& mtx_wakeUp) : cv_wakeUp(cv_wakeUp), mtx_wakeUp(mtx_wakeUp), b_stop(false), b_active(false), p_userTask(nullptr)
{}

/// <summary>
/// Метод основной работы. Запускается в другом потоке
/// </summary>
void task_t::Work()
{
	bool wakeUp = false; // признак на оповещение внешнего окружения о завершении таски

	while (!b_stop)
	{
		std::unique_lock<std::mutex> lock(mutex);
		if (!b_stop)
			cv_condition.wait(lock, [this]() { return b_active; });// ожидаем обновление задачи

		if (!b_stop && p_userTask != nullptr/* && b_active */) // если нет остановки потоков
		{
			p_userTask->Work(b_stop); // запускаем пользовательскую таску
			p_userTask = nullptr; // затираем выполненую таску
			b_active = false; // после пользовательской таски, отдыхаем
			wakeUp = true; // будем 
		}
		lock.unlock(); // отпускаем основной мьютекс
		// оповещение окружения о выполненой задачи
		if (wakeUp)
		{
			wakeUp = false;
			std::lock_guard<std::mutex> lock(mtx_wakeUp);
			cv_wakeUp.notify_one();
		}
	}
}

/// <summary>
/// Метод добавления таски
/// </summary>
/// <param name="userTask"> - указатель на объект пользовательской таски </param>
/// <returns> 1 - таска обновлена </returns>
bool task_t::UpdateTask(std::shared_ptr<ABStask> userTask)
{
	bool result = false;

	if (mutex.try_lock()) // пробуем захватить мьютекс
	{ // удача только в случае свободного потока
		if (!b_active && p_userTask == nullptr)
		{ // перепроверяем отуствие активности 
			p_userTask = userTask; // обновляем задачу
			b_active = true;
			cv_condition.notify_one();
			result = true;
		}
		mutex.unlock();
	}

	return result;
}

/// <summary>
/// Метод проверки активности
/// </summary>
/// <returns> 1 - таска активна </returns>
bool task_t::GetActive() const
{
	bool result = true;

	if (mutex.try_lock())
	{ // пробуем захватить мьютекс
		result = b_active;
		mutex.unlock();
	}
	// если мьютекс захвачен до нас, то задача активна
	return result;
}

/// <summary>
/// Метод окончательного выхода из таски
/// </summary>
void task_t::Stop()
{
	b_stop = true; // возвести флаг на остановку

	if (mutex.try_lock())
	{ // если захватили мьютекс, значит задача была не активна
		if (!b_active)
		{ // перепроверяем
			b_active = true; // запускаем
			cv_condition.notify_one();
		}
		mutex.unlock();
	}
}

/// <summary>
/// деструктор
/// </summary>
task_t::~task_t()
{}

/// <summary>
/// конструктор по умолчанию
/// </summary>
thread_t::thread_t(std::condition_variable& cv_wakeUp, std::mutex& mtx_wakeUp) : thread([this](){ task.Work(); }), task(cv_wakeUp, mtx_wakeUp)
{}

///деструктор
thread_t::~thread_t()
{
	task.Stop(); // тормозим таску
	thread.join(); // ждем окончания потока
}

/// <summary>
/// конструктор
/// </summary>
/// <param name="cv_wakeUp"> - условная переменная внешнего окружения - сигнал о выподнении задачи </param>
/// <param name="mtx_wakeUp"> - мьютексная защита переменной </param>
poolThread_t::poolThread_t(std::condition_variable& cv_wakeUp, std::mutex& mtx_wakeUp, const size_t SIZE) : SIZE(SIZE), free(false)
{
	v_thread.reserve(SIZE);
	v_taskID.reserve(SIZE);
	for (size_t index = 0; index < SIZE; ++index)
	{   // динамическое выделение памяти, где проиизодит запуск потоков
		v_thread.push_back(std::make_shared<thread_t>(cv_wakeUp, mtx_wakeUp));
		v_taskID.push_back(0);
	}
}

/// <summary>
/// Мето обновления таски
/// </summary>
/// <param name="userTask"> - указатель на объект пользовательской таски </param>
/// <returns> INVALID_TASK - нет свободных потоков; ID>INVALID_TASK - ID таски </returns>
bool poolThread_t::UpdateTask(std::shared_ptr<ABStask> userTask, taskID ID)
{
	for (size_t index = 0; index < SIZE; ++index) // ищем свободную таску
		if (!v_thread[index]->task.GetActive())
			if (v_thread[index]->task.UpdateTask(userTask)) // пробуем задать её
			{
				v_taskID[index] = ID; // сохраняем ID
				free = false;
				return true;
			}

	return free = false;
}

/// <summary>
/// Метод проверки активности таски
/// </summary>
/// <param name="ID"> - ID целевой таски </param>
/// <returns> 1 - таска активна </returns>
bool poolThread_t::GetActiveTask(taskID ID)
{
	for (size_t index = 0; index < SIZE; ++index)
		if (v_taskID[index] == ID)
			return v_thread[index]->task.GetActive();

	return false;
}

/// <summary>
/// метод опроса наличия свободного потока
/// </summary>
/// <returns> 1 - есть хотя бы один свободный поток </returns>
bool poolThread_t::GetFree() const
{
	bool result = free;
	if (!result)
		for (size_t index = 0; index < SIZE; ++index) // ищем свободную таску
			if (!v_thread[index]->task.GetActive())
				free = result = true;

	return result;
}

/// <summary>
/// метод работы управляющего потока
/// </summary>
void poolThread_manager_t::Update()
{
	while (!stop)
	{
		std::unique_lock<std::mutex> lock(mutex);
		if (!stop) // ожидаем сигнал по завершению работы / освобождению места в пуле
			cv_condition.wait(lock, [this]() { return stop || slave_pool.GetFree(); });
		if (!stop)
		{
			std::map<taskID, status_p_task_t>::iterator iter = m_task.begin();
			std::map<taskID, status_p_task_t>::iterator end = m_task.end();
			while (iter != end) // проходим по всему мапу по возрастанию taskID для соблюдения очередности поступления задач
				if (iter->second.status == EXCEPTION && slave_pool.UpdateTask(iter->second.p_task, iter->first))
				{ // если задача в очереди и она успешно зашла в пул - она уже активна
					iter->second.status = ACTIVE;
					++iter;
				}
				else if (iter->second.status == ACTIVE && !slave_pool.GetActiveTask(iter->first))
				{ // если задача числится активной, а пул говорит об обратном - задача выполнена 
					iter = m_task.erase(iter);
				}
				else // если не осталось свободных мест 
					++iter;
		}
	}
}

/// <summary>
/// конструктор по умолчанию
/// </summary>
poolThread_manager_t::poolThread_manager_t(const size_t SIZE) : slave_pool(cv_condition, mutex, SIZE), counter(0), master_thread([this]() { Update(); }), stop(false)
{}

/// <summary>
/// деструктор
/// </summary>
poolThread_manager_t::~poolThread_manager_t()
{   // ответствены за остановку управляющего потока
	stop = true;
	mutex.lock(); // будим его, если он спит
	cv_condition.notify_one();
	mutex.unlock();

	master_thread.join(); // ждем окончания потока
}

/// <summary>
/// метод добавления новой задачи
/// </summary>
/// <param name="p_task"> - smart_ptr на пользовательскую задачу </param>
/// <returns> присвоенный пользовательской задаче уникальный номер (дескриптор) </returns>
taskID poolThread_manager_t::AddTask(std::shared_ptr<ABStask> p_task)
{
	taskID result = ++counter;
	// добавляем в мап новую задачу
	std::lock_guard<std::mutex> lock(mutex);
	m_task[result].p_task = p_task;
	m_task[result].status = EXCEPTION;
	cv_condition.notify_one(); // будим управляющий поток

	return result;
}

/// <summary>
/// метод получения статуса задачи
/// </summary>
/// <param name="ID"></param>
/// <returns> NON_DEFINE (0) - не определено(невалидный taskID)
///           ACTIVE (1) - задача в процессе выполнения 
///           COMPLECTED (2) - задача выполнена
///           EXCEPTION (3) - задача в очереди на выполнение  </returns>
int poolThread_manager_t::GetStatusTask(taskID ID)
{
	int result = NON_DEFINE;

	if (ID < counter)
	{   // если аргумент валидный
		std::lock_guard<std::mutex> lock(mutex);
		if (m_task.find(ID) != m_task.end()) // ищем его
			result = m_task[ID].status;
		else // есди не нашли - задачп была выполнена
			result = COMPLECTED;
	}

	return result;
}
