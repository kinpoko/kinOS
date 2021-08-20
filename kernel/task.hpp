/**
 * @file task.hpp
 *
 * タスク管理，コンテキスト切り替えのプログラムを集めたファイル。
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <vector>
#include <string>

#include "error.hpp"
#include "message.hpp"
#include "paging.hpp"
#include "fat.hpp"

struct TaskContext {
  uint64_t cr3, rip, rflags, reserved1; // offset 0x00
  uint64_t cs, ss, fs, gs; // offset 0x20
  uint64_t rax, rbx, rcx, rdx, rdi, rsi, rsp, rbp; // offset 0x40
  uint64_t r8, r9, r10, r11, r12, r13, r14, r15; // offset 0x80
  std::array<uint8_t, 512> fxsave_area; // offset 0xc0
} __attribute__((packed));


using TaskFunc = void (uint64_t, int64_t);

class TaskManager;

struct FileMapping {
  int fd;
  uint64_t vaddr_begin, vaddr_end;
};

/**
 * @brief タスク(プロセス)のクラス
 * 
 */
class Task {
    public:
      static const int kDefaultLevel = 1;
      static const size_t kDefaultStackBytes = 8 * 4096;

      Task(uint64_t id);
      Task& InitContext(TaskFunc* f, int64_t data);
      TaskContext& Context();
      uint64_t& OSStackPointer();
      uint64_t ID() const;
      uint64_t PID() { return pid_; }
      Task& Sleep();
      Task& Wakeup();
      void SendMessage(const Message& msg);
      std::optional<Message> ReceiveMessage();
      std::vector<std::shared_ptr<::FileDescriptor>>& Files();
      uint64_t DPagingBegin() const;
      void SetDPagingBegin(uint64_t v);
      uint64_t DPagingEnd() const;
      void SetDPagingEnd(uint64_t v);
      uint64_t FileMapEnd() const;
      void SetFileMapEnd(uint64_t v);
      std::vector<FileMapping>& FileMaps();
      
      int Level() const { return level_; }
      bool Running() const { return running_; }

      Task& SetPID(uint64_t pid) { pid_ = pid; return *this; }

      void SetCommandLine(char* command_line) { command_line_ = command_line; }
      std::string GetCommandLine() { return command_line_; }

    private:
      uint64_t id_; // タスクのID
      uint64_t pid_{0}; // 親タスクのID 親は0
      std::string command_line_; // 実行しているコマンド
      std::vector<uint64_t> stack_;
      alignas(16) TaskContext context_;
      uint64_t os_stack_ptr_;
      std::deque<Message> msgs_;
      unsigned int level_{kDefaultLevel};
      bool running_{false};
      std::vector<std::shared_ptr<::FileDescriptor>> files_{};
      uint64_t dpaging_begin_{0}, dpaging_end_{0};
      uint64_t file_map_end_{0};
      std::vector<FileMapping> file_maps_{};

      Task& SetLevel(int level) { level_ = level; return *this; }
      Task& SetRunning(bool running) { running_ = running; return *this; }

      friend TaskManager;
};

/**
 * @brief タスクを管理するクラス
 * 
 */
class TaskManager {
  public:
    static const int kMaxLevel = 3;

    TaskManager();
    Task& NewTask();
    void SwitchTask(const TaskContext& current_ctx);

    void Sleep(Task* task);
    Error Sleep(uint64_t id);
    void Wakeup(Task* task, int level = -1);
    Error Wakeup(uint64_t id, int level = -1);
    Error SendMessage(uint64_t id, const Message& msg);
    Task& CurrentTask();
    
    void SetOsTaskId(uint64_t os_task_id) { os_task_id_ = os_task_id; }
    Error SendMessageToOs(const Message& msg);


    void Finish(int exit_code);
    WithError<int> WaitFinish(uint64_t task_id);

    Error RestartTask(uint64_t id);
    Error CreateAppTask(uint64_t pid, uint64_t cid);
    Task* FindTask (uint64_t id);
    uint64_t NumOfTask() { return latest_id_; } // タスクの数  

  private:
    std::vector<std::unique_ptr<Task>> tasks_{}; // タスクすべての配列
    uint64_t latest_id_{0};
    std::array<std::deque<Task*>, kMaxLevel + 1> running_{};
    int current_level_{kMaxLevel};
    bool level_changed_{false};
    std::map<uint64_t, int> finish_tasks_{}; // key: ID of a finished task
    std::map<uint64_t, Task*> finish_waiter_{}; // key: ID of a finished task
    
    uint64_t os_task_id_{0};

    void ChangeLevelRunning(Task* task, int level);
    Task* RotateCurrentRunQueue(bool current_sleep);
};

extern TaskManager* task_manager;

void InitializeTask();