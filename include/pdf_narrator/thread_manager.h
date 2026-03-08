#pragma once

#include <QObject>
#include <QThread>
#include <QHash>

enum class ThreadType
{
    PDFParser,
    TTSManager
};

class ThreadManager : public QObject
{
    Q_OBJECT

public:
    explicit ThreadManager(QObject *parent = nullptr);
    ~ThreadManager();

    void submitTask(ThreadType type, std::function<void()> task);
    int queuedTaskCount(ThreadType type) const;

private:
    void createThread(ThreadType type, const QString &name);
    struct ThreadContext
    {
        QThread thread;
        QObject worker;
        std::atomic<int> pendingTasks{0};
    };

    QHash<ThreadType, ThreadContext *> m_threads;
};
