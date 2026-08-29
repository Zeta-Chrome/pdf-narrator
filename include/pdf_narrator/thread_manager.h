#pragma once

#include <QObject>
#include <QThread>
#include <QHash>

enum class ThreadType : uint8_t { PDFParser, TTSManager };

struct ThreadContext {
	QThread thread;
	QObject worker;
	std::atomic<int> pendingTasks{ 0 };
};

class ThreadManager : public QObject {
	Q_OBJECT

public:
	explicit ThreadManager(QObject *parent = nullptr);
	~ThreadManager();

	void submitTask(ThreadType type, std::function<void()> task);
	void shutdown();

private:
	void createThread(ThreadType type, const QString &name);

	QHash<ThreadType, ThreadContext *> m_threads;
};
