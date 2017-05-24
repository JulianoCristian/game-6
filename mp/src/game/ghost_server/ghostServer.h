#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>
#include <stdio.h>
#include <thread>
#include <time.h>
#include <vector>

#include "mom_ghostdefs.h"
#include "mom_shareddefs.h"

// single-file UDP library
#include "zed_net.h"

#ifdef _WIN32
#include <Windows.h>
#define thread_local __declspec(thread)
#else
#define _snprintf snprintf
#define thread_local __thread
#endif

#define DEFAULT_MAP "triggertests"
#define SECONDS_TO_TIMEOUT 10
#define NEW_MAP_CMD "MOMENTUM_QUEUE_NEWMAP"
#define NEW_APPEARENCES_CMD "MOMENTUM_QUEUE_NEWAPPS"

template <class T> class SafeQueue;
struct playerData;
class CMOMGhostServer
{
    // EVERYTHING is static because there is no server object, there is only one server.
  public:
    static void handlePlayer(playerData *newPlayer);
    static void handleConsoleInput();
    static int runGhostServer(const unsigned short port, const char *mapname);
    static void newConnection(zed_net_socket_t socket, zed_net_address_t address);
    static void acceptNewConnections();
    static void disconnectPlayer(playerData *player);
    static void sendNewAppearances(playerData *player);
    static void conMsg(const char *msg, ...);

    static volatile int numPlayers;
    static bool m_bShouldExit;
    static std::vector<playerData *> m_vecPlayers;
    static SafeQueue<char *> m_sqEventQueue;
    static std::mutex m_vecPlayers_mutex;
    static std::mutex m_bShouldExit_mutex;

  private:
    static zed_net_socket_t m_Socket;
    static int m_iTickRate;
    static char m_szMapName[96];
    static const std::chrono::seconds m_secondsToTimeout;
};

// A threadsafe-queue.
template <class T> class SafeQueue
{
  public:
    SafeQueue() : m_queue(), m_mtx(), m_condVar() {}

    ~SafeQueue() {}

    // Add an element to the queue.
    void enqueue(T t)
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_queue.push(t);
        m_condVar.notify_one();
    }

    // Get the "front"-element.
    // If the queue is empty, wait till a element is avaiable.
    T dequeue()
    {
        std::unique_lock<std::mutex> lock(m_mtx);
        while (m_queue.empty())
        {
            // release lock as long as the wait and reaquire it afterwards.
            m_condVar.wait(lock);
        }
        T val = m_queue.front();
        m_queue.pop();
        return val;
    }
    int numInQueue()
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        return m_queue.size();
    }

  private:
    std::queue<T> m_queue;
    mutable std::mutex m_mtx;
    std::condition_variable m_condVar;
};
struct playerData
{
    int clientIndex;
    ghostNetFrame_t currentFrame;
    ghostAppearance_t currentLooks;
    zed_net_socket_t remote_socket;
    zed_net_address_t remote_address;
    playerData(zed_net_socket_t socket, zed_net_address_t addr, int idx)
        : clientIndex(idx), remote_socket(socket), remote_address(addr)
    {
    }
};
inline std::string currentDateTime()
{
    time_t now = time(0);
    struct tm tstruct;
    char buf[80];
    tstruct = *localtime(&now);
    // Visit http://en.cppreference.com/w/cpp/chrono/c/strftime
    // for more information about date/time format
    strftime(buf, sizeof(buf), "%X", &tstruct);

    return std::move(std::string(buf));
}