#include "thread_manager.h"

ThreadManager::ThreadManager(QObject *parent)
	: QObject(parent)
{
	createThread(ThreadType::PDFParser, "PDFThread");
	createThread(ThreadType::TTSManager, "TTSThread");
	createThread(ThreadType::PlaybackEngine, "PlaybackEngine");
}

ThreadManager::~ThreadManager()
{
	for (auto ctx : m_threads) {
		ctx->thread.quit();
		ctx->thread.wait();
		delete ctx;
	}
}

void ThreadManager::createThread(ThreadType type, const QString &name)
{
	auto *ctx = new ThreadContext;
	ctx->thread.setObjectName(name);
	ctx->worker.moveToThread(&ctx->thread);
	ctx->thread.start();
	m_threads.insert(type, ctx);
};

int ThreadManager::queuedTaskCount(ThreadType type) const
{
	auto ctx = m_threads.value(type, nullptr);
	if (!ctx)
		return 0;

	return ctx->pendingTasks.load();
}

void ThreadManager::submitTask(ThreadType type, std::function<void()> task)
{
	auto ctx = m_threads.value(type, nullptr);
	if (!ctx)
		return;

	ctx->pendingTasks++;
	QMetaObject::invokeMethod(
		&ctx->worker,
		[ctx, task = std::move(task)]() {
			task();
			ctx->pendingTasks--;
		},
		Qt::QueuedConnection);
}
