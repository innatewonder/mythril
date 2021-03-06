#include "CommonPrecompiled.h"
#include "HBThread.h"

ThreadPool::ThreadPool() : 
  m_exitAll(false), m_jobCount(0)
{
  unsigned numCores = std::thread::hardware_concurrency();
  for(unsigned i = 0; i < numCores; ++i)
  {
    m_threads.push_back(std::thread(&ThreadPool::ThreadMainFunc, this));
  }
}

ThreadPool::~ThreadPool()
{
  m_exitAll = true;
  m_semaphore.Notify(m_threads.size());

  for(unsigned i = 0; i < m_threads.size(); ++i)
  {
    m_threads[i].join();
  }

  m_jobs.ClearWithDelete();
  m_finished.ClearWithDelete();
}

void ThreadPool::Update()
{
  unsigned numJobs = m_finished.Size();
  while(numJobs)
  {
    ThreadJob* job = m_finished.Pop();
    job->PostExecute();
    delete job;
    --numJobs;
    --m_jobCount;
  }
}

void ThreadPool::AddJob(ThreadJob* j)
{
  ++m_jobCount;
  m_jobs.Push(j);
  m_semaphore.Notify();
}

void ThreadPool::ClearJobs()
{
  m_jobCount = 0;
  m_jobs.Clear();
  m_semaphore.Clear();
}

u32 ThreadPool::NumJobs()
{
  return m_jobCount;
}

bool ThreadPool::HasJobs()
{
  unsigned workingJobs = m_jobs.Size() + m_finished.Size();
  if(m_jobCount < workingJobs)
  {
    m_jobCount = workingJobs;
  }
  return m_jobCount > 0;
}

u32 ThreadPool::NumCores() const
{
  return std::thread::hardware_concurrency();
}

void ThreadPool::SyncPoint()
{
  while(HasJobs())
    Update();
}

void ThreadPool::ThreadMainFunc()
{
#ifdef TIME_THREADS
  Core::Timer t;
#endif
  while(true)
  {
    m_semaphore.Wait();

    if(m_exitAll)
      return;

    ThreadJob* job = m_jobs.Pop();
#ifdef TIME_THREADS
    t.Start();
#endif
    job->Execute();
#ifdef TIME_THREADS
    LOG("ThreadJob " << job->Name() << " execute took " << t.GetDT_Sec());
    t.Step();
#endif

    if(!job->IsPostOnMainThread())
    {
      job->PostExecute();
      --m_jobCount;
      delete job;
    }
    else
    {
      m_finished.Push(job);
    }
#ifdef TIME_THREADS
    //LOG("ThreadJob " << job->Name() << " postExecute took " << t.GetDT_Sec());
    t.Stop();
#endif
  }
}
