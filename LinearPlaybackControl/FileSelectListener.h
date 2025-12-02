/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <thread>
#include <functional>
#include <fstream>
#include <atomic>
#include <vector>

class FileSelectListener
{
public:
    FileSelectListener(const std::string &file,
                       uint32_t bufSize,
                       std::function<void(const std::string&)> func)
            : mFile(file)
            , mBufSize(bufSize)
            , mFunc(func)
            , mStop(false)
    {
        mThread = std::shared_ptr<std::thread>(
                new std::thread(&FileSelectListener::pollLoop, this));
    }

    ~FileSelectListener()
    {
        mStop = true;
        if (mThread != nullptr && mThread->joinable()) {
            mThread->join();
        }
    }

private:
    FileSelectListener(const FileSelectListener&) = delete;
    FileSelectListener& operator=(const FileSelectListener&) = delete;

    std::string mFile;
    uint32_t mBufSize;
    std::function<void(const std::string&)> mFunc;
    // FIX(Coverity): Use std::atomic for mStop to prevent race condition
    // Reason: mStop accessed from multiple threads without synchronization
    // Impact: No API signature changes. Made mStop atomic for thread safety.
    std::atomic<bool> mStop;

    std::shared_ptr<std::thread> mThread;

    inline bool file_exists(const std::string &name) {
        std::ifstream f(name.c_str());
        f.close();
        return f.good();
    }

    void pollLoop() {
        // FIX(Coverity): Allocate buffer on heap to prevent potential stack overflow
        // Reason: mBufSize could be large, fixed-size stack array is risky
        // Impact: No API signature changes. Use heap allocation for safety.
        std::vector<char> buf(mBufSize);
        int fd = -1;
        while (!mStop) {
            if (!file_exists(mFile)) {
                syslog(LOG_ERR, "%s is missing. Retry in 2 seconds", mFile.c_str());
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }

            fd = open(mFile.c_str(), O_RDONLY);
            if (fd < 0) {
                syslog(LOG_ERR, "Could not open %s", mFile.c_str());
                continue;
            }

            while (!mStop) {
                fd_set rfds;
                struct timeval timeout;

                // FIX(Coverity): Validate fd before FD_SET to prevent buffer overflow
                // Reason: FD_SET doesn't check if fd >= FD_SETSIZE
                // Impact: No API signature changes. Added bounds check for safety.
                if (fd >= FD_SETSIZE) {
                    syslog(LOG_ERR, "File descriptor %d exceeds FD_SETSIZE", fd);
                    break;
                }

                FD_ZERO(&rfds);
                FD_SET(fd, &rfds);

                /* Initialize the timeout data structure. */
                timeout.tv_sec = 2;
                timeout.tv_usec = 0;

                int res = select(fd + 1, &rfds, nullptr, nullptr, &timeout);
                if (res < 0) {
                    syslog(LOG_ERR, "Calling select on %s failed", mFile.c_str());
                    break;
                }

                if (!FD_ISSET(fd, &rfds)) {
                    continue;
                }
                lseek(fd, 0, SEEK_SET);

                res = read(fd, buf.data(), mBufSize);
                
                if (res < 0) {
                    if (errno == ENOTCONN || errno == EBADF || errno == ECONNRESET) {
                        syslog(LOG_ERR, "Socket error for %s", mFile.c_str());
                        break;
                    } else {
                        syslog(LOG_ERR, "Failed to read data from %s", mFile.c_str());
                        std::this_thread::sleep_for(std::chrono::seconds(2));
                        continue;
                    }
                } else if (res > 0 && static_cast<size_t>(res) <= mBufSize) {
                    mFunc(std::string(buf.data(), res));
                }
            }
            close(fd);
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
};
