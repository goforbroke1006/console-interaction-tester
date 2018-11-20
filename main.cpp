#include <iostream>
#include <zconf.h>
#include <sys/wait.h>
#include <cstring>
#include <thread>
#include <signal.h>
#include <chrono>

#include "strings.h"

constexpr auto PIPE_READ = 0;
constexpr auto PIPE_WRITE = 1;

using namespace std;
using namespace std::chrono;

constexpr int MILLISECONDS_TO_MICROSECONDS = 1000;

int main(int argc, char **argv) {

    if (argc == 2 && (
            strcmp(argv[1], "?") == 0
            || strcmp(argv[1], "--help") == 0
            || strcmp(argv[1], "-h") == 0
    )) {
        cout << "Usage: ./console-interaction-tester "
                "<SOME_BIN_TEST_SUBJECT> <TEST_INPUT> <EXPECTED_OUTPUT> <TIMEOUT_IN_MILLISECONDS>";
        return 0;
    }

    string binFilename;
    string testInput;
    string expectedOutput;
    unsigned int timeout = 0;

    if (argc >= 4) {
        binFilename = argv[1];
        testInput = argv[2];
        expectedOutput = argv[3];
        timeout = atoi(argv[4]);
    } else {
        cout << "Enter subject binary path: ";
        getline(cin, binFilename);

        cout << "Enter test input: ";
        getline(cin, testInput);

        cout << "Enter expected output: ";
        getline(cin, expectedOutput);

        cout << "Timeout: ";
        cin >> timeout;
    }

    int inputFd[2];
    int outputFd[2];
    char nChar;

    if (pipe(inputFd) < 0) {
        perror("input pipe");
        exit(EXIT_FAILURE);
    }

    if (pipe(outputFd) < 0) {
        close(inputFd[PIPE_READ]);
        close(inputFd[PIPE_WRITE]);
        perror("output pipe");
        exit(EXIT_FAILURE);
    }

    pid_t childPid = fork();
    if (childPid < 0) {
        close(inputFd[PIPE_READ]);
        close(inputFd[PIPE_WRITE]);
        close(outputFd[PIPE_READ]);
        close(outputFd[PIPE_WRITE]);
        perror("fork() error");
        exit(-1);
    }

    int status;
    string result;
    pid_t pid;

    milliseconds timeBefore;
    milliseconds timeAfter;

    if (childPid == 0) {
//        printf("I'm child %d, parent is %d\n", getpid(), getppid());

        if (dup2(inputFd[PIPE_READ], STDIN_FILENO) == -1) { // redirect stdin
            exit(errno);
        }
        if (dup2(outputFd[PIPE_WRITE], STDOUT_FILENO) == -1) { // redirect stdout
            exit(errno);
        }
        if (dup2(outputFd[PIPE_WRITE], STDERR_FILENO) == -1) { // redirect stderr
            exit(errno);
        }
        close(inputFd[PIPE_READ]);
        close(inputFd[PIPE_WRITE]);
        close(outputFd[PIPE_READ]);
        close(outputFd[PIPE_WRITE]);

        timeBefore = duration_cast<milliseconds>(system_clock::now().time_since_epoch());

        const char *szCommand = binFilename.data();
        char *const aArguments[] = {(char *const) binFilename.data(), nullptr};
        char *const aEnvironment[] = {nullptr};
        int nResult = execve(szCommand, aArguments, aEnvironment);
        exit(nResult);
    }

    thread cThread;

    if (childPid > 0) {
//        printf("I'm paretn %d, child is %d\n", getpid(), childPid);

        close(inputFd[PIPE_READ]);
        close(outputFd[PIPE_WRITE]);

//        waitpid(childPid, &status, WNOHANG);
//        printf("Child ready for input\n");

//        if ((pid = wait(&status)) < 0) {
//            perror("wait");
//            exit(1);
//        }

        const char *input = (testInput + "\n").c_str();
        write(inputFd[PIPE_WRITE], input, strlen(input));
        while (read(outputFd[PIPE_READ], &nChar, 1) == 1) {
            result += nChar;
        }
    }

//    wait(nullptr);

    timeAfter = duration_cast<milliseconds>(system_clock::now().time_since_epoch());

    const milliseconds &delta = timeAfter - timeBefore;
    if (delta.count() > timeout) {
        cerr << "time limit exceeded (" << delta.count() << ")" << endl;
        return -1;
    }

    result = trim(result);

//    waitpid(childPid, &status, WUNTRACED);
//    printf("Child finish with status %d\n", status);
//    printf("Child out data: %s\n", result.c_str());

    if (expectedOutput == result) {
        printf("OK\n");
    } else {
        fprintf(stderr,
                "Assertion failed!\n  Expected: '%s'\n  Actual:   '%s'",
                expectedOutput.c_str(),
                result.c_str()
        );
        return -1;
    }

    return 0;
}
