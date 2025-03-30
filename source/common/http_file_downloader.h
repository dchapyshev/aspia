//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#ifndef COMMON_HTTP_FILE_DOWNLOADER_H
#define COMMON_HTTP_FILE_DOWNLOADER_H

#include "base/macros_magic.h"
#include "base/task_runner.h"
#include "base/memory/byte_array.h"
#include "base/threading/simple_thread.h"

namespace common {

class HttpFileDownloader
{
public:
    HttpFileDownloader();
    ~HttpFileDownloader();

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onFileDownloaderError(int error_code) = 0;
        virtual void onFileDownloaderCompleted() = 0;
        virtual void onFileDownloaderProgress(int percentage) = 0;
    };

    void start(std::u16string_view url,
               std::shared_ptr<base::TaskRunner> owner_task_runner,
               Delegate* delegate);
    const base::ByteArray& data() const { return data_; }

private:
    void run();

    static size_t writeDataCallback(void* ptr, size_t size, size_t nmemb, HttpFileDownloader* self);
    static int progressCallback(
        HttpFileDownloader* self, double dltotal, double dlnow, double ultotal, double ulnow);

    base::SimpleThread thread_;

    class Runner;
    std::shared_ptr<Runner> runner_;

    std::u16string url_;
    base::ByteArray data_;

    DISALLOW_COPY_AND_ASSIGN(HttpFileDownloader);
};

} // namespace common

#endif // COMMON_HTTP_FILE_DOWNLOADER_H
