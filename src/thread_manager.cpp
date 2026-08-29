#include "thread_manager.h"
#include "app_controller.h"
#include <qdebug.h>

ThreadManager::ThreadManager(QObject *parent)
	: QObject(parent)
{
	createThread(ThreadType::PDFParser, "PDFThread");
	createThread(ThreadType::TTSManager, "TTSThread");
}

ThreadManager::~ThreadManager()
{
	shutdown();
}

void ThreadManager::createThread(ThreadType type, const QString &name)
{
	auto *ctx = new ThreadContext;
	ctx->thread.setObjectName(name);
	ctx->worker.moveToThread(&ctx->thread);
	ctx->thread.start();
	m_threads.insert(type, ctx);
};

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

void ThreadManager::shutdown()
{
	for (auto ctx : m_threads) {
		ctx->worker.disconnect();
		ctx->thread.quit();
		ctx->thread.wait();
		delete ctx;
	}
}
