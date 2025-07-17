// PerfCounter.h

#ifndef countof
#define countof(arr) (sizeof(arr)/sizeof(arr[0]))
#endif
#include <stdio.h>

#ifndef _WIN32
#include <time.h>

static struct ::timespec timespec_substract(struct ::timespec const& left, struct ::timespec const& right)
{
    struct ::timespec res;
    if ((left.tv_nsec - right.tv_nsec) < 0)
    {
        res.tv_sec = left.tv_sec - right.tv_sec - 1;
        res.tv_nsec = 1000000000 + left.tv_nsec - right.tv_nsec;
    }
    else
    {
        res.tv_sec = left.tv_sec - right.tv_sec;
        res.tv_nsec = left.tv_nsec - right.tv_nsec;
    }
    return res;
}

static struct ::timespec timespec_add(struct ::timespec const& x, struct ::timespec const& y)
{
    struct ::timespec res;
    if ((x.tv_nsec + y.tv_nsec) >= 1000000000)
    {
        res.tv_sec = x.tv_sec + y.tv_sec + 1;
        res.tv_nsec = x.tv_nsec + y.tv_nsec - 1000000000;
    }
    else
    {
        res.tv_sec = x.tv_sec + y.tv_sec;
        res.tv_nsec = x.tv_nsec + y.tv_nsec;
    }
    return res;
}

static bool timespec_is_zero(struct ::timespec const& x)
{
    return !(x.tv_sec || x.tv_nsec);
}

class CPerfCounter
{
    struct ::timespec m_nLastStarted;
    struct ::timespec m_nCount;
	long long m_nMeasureCount; // how many times Resume[/Pause] was called
	char m_szName[32];
public:
	CPerfCounter(char const* pszName) : m_nMeasureCount(0)
	{
        m_nLastStarted.tv_sec = 0;
        m_nLastStarted.tv_nsec = 0;

        m_nCount.tv_sec = 0;
        m_nCount.tv_nsec = 0;

		strncpy(m_szName, pszName, countof(m_szName));
		m_szName[countof(m_szName) - 1] = 0;
	}
    struct ::timespec const& GetTotalCount() const
	{
		return m_nCount;
	}
    size_t GetSimpleCount() const
    {
        return (size_t)(m_nCount.tv_sec*1000000000 + m_nCount.tv_nsec);
    }
    double GetSeconds() const
    {
        return m_nCount.tv_sec + m_nCount.tv_nsec/1000000000.;
    }

	void Resume()
	{	
		if (timespec_is_zero(m_nLastStarted))
        {
            ::clock_gettime(CLOCK_REALTIME, &m_nLastStarted);
            ++m_nMeasureCount;
        }
		else
			printf("ERROR: the counter [%s] has been resumed already\n", m_szName);
	}
	void Pause()
	{
		if (!timespec_is_zero(m_nLastStarted))
		{
            struct ::timespec nCurrent;
            ::clock_gettime(CLOCK_REALTIME, &nCurrent);
            m_nCount = timespec_add(m_nCount, timespec_substract(nCurrent, m_nLastStarted));
			m_nLastStarted.tv_sec = 0;
            m_nLastStarted.tv_nsec = 0;
		}
		else
			printf("ERROR: the counter [%s] has been paused already\n", m_szName);
	}
	void Print() const
	{
        printf("[%s]: %ld.%09ld s; measure count=%lld%s\n", m_szName, GetTotalCount().tv_sec, GetTotalCount().tv_nsec,
			m_nMeasureCount, !timespec_is_zero(m_nLastStarted) ? " (still running)" : "");
	}
	void Reset()
	{
        m_nLastStarted.tv_sec = 0;
        m_nLastStarted.tv_nsec = 0;

        m_nCount.tv_sec = 0;
        m_nCount.tv_nsec = 0;

		m_nMeasureCount = 0;
	}
};
#else
//#include <undefwin.h>
#include <Windows.h>
class CPerfCounter
{
	__int64 m_nLastStarted;
	__int64 m_nCount;
	__int64 m_nMeasureCount; // how many times Resume[/Pause] was called
	char m_szName[32];
public:
	CPerfCounter(char const* pszName) : m_nLastStarted(0), m_nCount(0), m_nMeasureCount(0)
	{
		strncpy(m_szName, pszName, countof(m_szName));
		m_szName[countof(m_szName) - 1] = 0;
	}
	CPerfCounter(char const* pszName, __int64 nLastStarted, __int64 nCount, __int64 nMeasureCount) : m_nLastStarted(nLastStarted), m_nCount(nCount), m_nMeasureCount(nMeasureCount)
	{
		strncpy(m_szName, pszName, countof(m_szName));
		m_szName[countof(m_szName) - 1] = 0;
	}
	__int64 GetTotalCount() const
	{
		if (!m_nLastStarted)
			return m_nCount;
		else
		{
			__int64 nCurrent = 0;
			::QueryPerformanceCounter((LARGE_INTEGER*)(&nCurrent));
			return m_nCount + nCurrent - m_nLastStarted;
		}
	}
    size_t GetSimpleCount() const
    {
        return (size_t)GetTotalCount();
    }

    double GetSeconds() const
    {
        __int64 nFreq = 0;
        ::QueryPerformanceFrequency((LARGE_INTEGER*)(&nFreq));
        return nFreq ? (double)GetTotalCount()/(double)nFreq : -1.;
    }

	void Resume()
	{	
		if (!m_nLastStarted)
		{
			::QueryPerformanceCounter((LARGE_INTEGER*)(&m_nLastStarted));
			++m_nMeasureCount;
		}
		else
			printf("ERROR: the counter [%s] has been resumed already\n", m_szName);
	}
	void Pause()
	{
		if (m_nLastStarted)
		{
			__int64 nCurrent = 0;
			::QueryPerformanceCounter((LARGE_INTEGER*)(&nCurrent));
			m_nCount += nCurrent - m_nLastStarted;
			m_nLastStarted = 0;
		}
		else
			printf("ERROR: the counter [%s] has been paused already\n", m_szName);
	}
	void Print() const
	{
		__int64 nFreq = 0;
		::QueryPerformanceFrequency((LARGE_INTEGER*)(&nFreq));
		printf("[%s]: %I64d (%I64d ms); measure count=%I64d%s\n", m_szName, GetTotalCount(), GetTotalCount()/(nFreq/1000),
			m_nMeasureCount, m_nLastStarted ? " (still running)" : "");
	}
	void Reset()
	{
		m_nLastStarted = 0;
		m_nCount = 0;
		m_nMeasureCount = 0;
	}
};
#endif

class CPCount
{
	CPerfCounter& m_count;
public:
	CPCount(CPerfCounter& count) : m_count(count)
	{
		m_count.Resume();
	}
	~CPCount()
	{
		m_count.Pause();
	}
};