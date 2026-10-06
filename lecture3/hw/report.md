# 项目理解报告

请尽量使用自己的语言回答以下问题。可以引用少量关键代码或伪代码，但不要只粘贴实现。
完成一节后删除该节末尾的待填写标记；本地检查会拒绝仍有未完成章节的报告。

## 1. 图像生命周期与所有权

解释本项目中图像源为什么会复用缓冲区，以及 `cv::Mat` 的普通复制对底层像素数据
意味着什么。说明你的修改让一个 `Frame` 在进入队列后拥有什么，并解释为何后续读取
不会再改变它。

1. 这里的缓冲区(buffer_)是ImageSequenceSource的一个成员变量，它在整个对象生命周期内只有一份。所以每次调用next()，都通过执行raw.copyTo(buffer_)，把新图写入这块固定的内存，而不是每一帧都新建。至于为什么要用这个缓冲区而不是直接raw.copyTo(frame.image),了解到是为了模拟真实相机内部的缓冲区。
2. 'cv::Mat'的普通复制是浅拷贝，了解到浅拷贝时只会复制矩阵头，不复制像素数据，而会让它们都指向同一块像素数据，同时把内部引用加1。原代码frame.image = buffer_;是浅拷贝,frame.image和buffer_共享像素内存，在下一次next()覆盖buffer_的时候，队列中的frame的图像内容也会跟着改变。因为我们后续是多线程工作，所以在消费前我们是不能把Frame的图像内容改变的。
3. 将原代码改成buffer_.copyTo(frame.image);这样的深拷贝，让这个Frame拥有自己的独立的一块像素内存，和buffer_不再共享。进入队列后Frame拥有一个独立的cv::Mat头，一块独立的像素内存，一个独立的id，一个独立的expected_checksum。
4. 因为在深拷贝以后，frame.image和buffer_是两块不同的内存，后续的next()的调用只会复用buffer_，而与frame.image无关。

## 2. 并发处理与恰好一次

结合 `BlockingQueue` 的 `push`、`pop` 和 `close` 行为，解释多个 worker 如何分工。
为什么你的实现既不会漏掉已经入队的帧，也不会重复处理同一帧？输入耗尽时，正在等待
以及仍在处理数据的 worker 分别会怎样？

1. pipeline::start()先创建worker_count个worker线程，每个都在跑workerLoop,循环调用queue_.pop(frame)。所有的worker都阻塞在同一个ready_上，等待同一个队列。生产者调用push函数，在锁内把帧压入quene_，锁外用notify_one唤醒一个等待的worker，被唤醒后重新拿锁检查谓词为真后取走队首元素。当确定生产者不会再push新数据的时候调用close()，唤醒所有等待的worker，队列中的帧全部取完处理完后，worker陆续返回false退出。
2. 因为调用close()，只是在确定生产者不会再push的时候把close_置为true,并不会清空队列。而pop的等待谓词是close_||!queue_.empty(),所以即使队列关闭，只要还有元素，pop仍然会返回true并取出元素。只有当队列真正为空的时候，pop才会返回false，所以不会漏掉已经入队的帧。
3. 因为pop有std::unique_lock< std::mutex> lock(mutex_);pop的取元素操作在锁内是原子的，同一个时刻只有一个worker能够拿到锁，拿到锁后的worker取走队首，并立刻把它从队列弹出。即使是调用close(),其他worker也被唤醒，但也只能等待锁释放，队首已经变成下一个元素或队列为空。所以不会重复处理同一帧。
4. 输入耗尽时，next()返回false后退出循环，调用queue.close()。正在等待的worker原本阻塞在ready_.wait，close()中的notify_all()将它们全部唤醒，唤醒后检查谓词，因为close_为真，所以不再阻塞，进入临界区，如果queue_为空，返回false,退出循环，如果不为空，继续取，直到队列为空；仍在处理数据的worker不会被close打断，继续完成操作后回到pop，如果队列为空返回false退出，不为空继续取直到队列为空。

## 3. 共享统计数据

指出哪些线程会读写 `Statistics`。解释原实现中的竞争为什么可能导致错误结果，并说明
你的同步方案提供了什么保证。还应说明取得快照时为什么是安全的。

1. 生产者线程调用onProduced(),每个worker线程调用onProcessed()、onSaved()、onCorrupted()；主线程通过Pipeline::statistics() 调用 snapshot()。
2. 因为原实现的deliberatelySlowIncrement先读旧值，休眠一段时间后，在写入旧值加一，这不是原子操作，所以多个线程可能会同时读到相同的旧值，会导致错误结果，计数会偏小。
3. 因为我在Statistics内部加入一个std::mutex，然后让所有的更新函数和snapshot()都使用std::lock_guard< std::mutex>来包住eliberatelySlowIncrement，从而实现互斥，让同一时刻只有一个线程在修改计数器。
4. 因为取得快照是在锁内一次性读取四个计数器，保证它们来自同一时刻，避免有的是旧值有的是新值，获得四个计数一致的状态。

## 4. 线程关闭协议

分别描述以下两条路径中的事件顺序，并解释为什么不会发生 `std::terminate`、悬空访问
或永久等待：

1. 调用者执行 `start()` 后显式调用 `wait()`；
2. 调用者执行 `start()` 后不调用 `wait()`，直接让 `Pipeline` 析构。

如果你的实现允许某个生命周期方法被重复调用，也请说明其行为；如果不允许，请说明前置条件。

#### 一、 
1. 调用者执行start(),创建输出目录；创建worker线程，进入workerLoop，此时队列为空，所有 worker 阻塞在 ready_.wait 上；创建producer线程，进入producerLoop。
2. 生产者循环：source_->next(frame)依次读出图片，statistics_.onProduced()，queue_.push(std::move(frame))，每次 push 唤醒一个 worker。
3. worker被唤醒后从pop取帧，做校验，processor_.process、imwrite、更新统计。处理完再回到 pop 等待下一帧。
4. 输入耗尽，source_->next()返回false，生产者退出循环，调用queue_.close。close() 在锁内设 closed_ = true，锁外 notify_all() 唤醒所有等待的 worker。生产者线程函数返回，线程结束。
5. 调用者显式调用wait(),先 producer_.join()。生产者已经结束，join 立即返回。再依次 worker_.join()。每个 worker 要么刚从 pop 返回 false 退出，要么处理完当前帧后回到 pop，发现队列已空且 closed_，返回 false 退出。join 依次返回。
6. wait()返回，所有线程结束，Pipeline对象仍然可以继续调用statistics()读快照。

**为什么不会 std::terminate？**
因为std::terminate 在 std::thread 析构时如果线程仍joinable()使才会被调用。wait() 已经 join 了所有线程，producer_ 和 workers_ 都不再 joinable()，所以之后析构时 if (thread.joinable()) 为假，跳过 join，不会触发 terminate。<br>
**为什么不会悬空访问？**
  因为join 保证线程函数已经返回，线程不再访问任何成员。wait() 返回后，所有线程都已停止，但是对象成员仍然有效。<br>
**为什么不会永久等待？**
  因为对于生产者next() 读的是有限的图片序列，最终返回 false，必然退出。对于worker，close() 一定在生产者退出前执行，notify_all 唤醒所有等待者，因为pop 谓词 closed_ || !queue_.empty() 为真，所以worker不会永久阻塞在 wait 上。wait() 里的 join执行后所有线程都会结束，join 必然返回。
#### 二、
1. 调用者执行start()，线程启动，过程同上。
2. 调用者离开作用域后，Pipeline析构函数开始执行，执行queue_.close()。在锁内设 closed_ = true，锁外 notify_all()。
3. 如果producer线程仍在运行，可能正在调用queue_.push(frame)。但是因为close_已经为true，所以push会直接返回，不会插入新元素，所以不会阻塞。producer 继续循环读取剩余图像，直到 source_->next 返回 false，然后再次调用 queue_.close()，producer 线程函数返回，join返回。
4. worker同上，被唤醒后如果队列中还有元素，pop 返回 true，worker 继续处理；如果队列为空且 closed_ 为真，pop 返回 false，worker 退出循环。
5. 析构函数检查 producer_.joinable()，如果为真则 join producer，等待 producer 线程结束。producer 会自然结束，因为 source_ 最终耗尽，且 push 在关闭后不会阻塞。join依次返回。
6. 析构函数体结束，各个成员按声明逆序销毁。所有线程停止。

**为什么不会 std::terminate？**
  因为析构函数在成员销毁前完成了所有 join，producer_ 和 workers_ 的 joinable() 都为假，所以std::thread 析构时不会 terminate。<br>
**为什么不会悬空访问？**
  因为析构的顺序是先停止线程再销毁成员。queue_.close() 在最前面，保证 worker 不会卡在 pop 上；join 保证线程函数完全返回后，才开始销毁成员。所以不会发生线程仍然运行但是成员已经被销毁的情况。<br>
**为什么不会永久等待？**
  因为queue_.close() 会唤醒所有等待者，pop 的谓词因 closed_ 为真而成立，不会永久阻塞；push 在关闭后会直接返回，不会阻塞 producer；worker 线程要么在 pop 上被唤醒退出，要么处理完当前帧后退出。
#### 三、
start():不允许重复调用。每次调用都会向workers_里面追加新的线程、覆盖producer_。如果producer_已经有了一个joinable的线程，会触发std::terminate。前置条件是 start() 只调用一次，且要在 wait() 或析构之前。<br>
wait():可以重复调用。 第一次调用后所有线程都已 join，joinable() 返回 false；第二次调用时所有 if (thread.joinable()) 都为假，函数直接返回，是空操作。<br>
statistics()：可以重复调用。 它只调用 statistics_.snapshot()，会在锁内读四个计数器，不依赖线程的状态，也不会修改成员。
