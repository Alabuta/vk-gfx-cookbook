#include <vector>
#include <print>

#include <taskflow/taskflow.hpp>
#include <taskflow/algorithm/for_each.hpp>

int main()
{
    std::vector items{1, 2, 3, 4, 5, 6, 7, 8};

    tf::Taskflow taskflow;

    auto task = taskflow.for_each_index(
        0u,
        static_cast<uint32_t>(items.size()),
        1u,
        [&](auto i)
        {
            std::print("{0}", items[i]);
        }).name("for_each_index");

    taskflow.emplace([] { std::println("\nS - Start"); }).name("S").precede(task);
    taskflow.emplace([] { std::println("\nT - End"); }).name("T").succeed(task);

    {
        std::ofstream os(".cache/taskflow.dot");
        taskflow.dump(os);
    }

    tf::Executor executor;
    executor.run(taskflow).wait();
}
