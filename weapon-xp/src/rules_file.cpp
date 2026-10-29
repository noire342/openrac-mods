// SPDX-License-Identifier: GPL-3.0-or-later
#include "rules_file.hpp"

#include <cerrno>
#include <fcntl.h>
#include <fstream>
#include <iterator>

#include <sys/stat.h>
#if defined(_WIN32)
#include <io.h>
#include <share.h>
#else
#include <unistd.h>
#endif

namespace weaponxp {
bool read_rules_file(
    const std::filesystem::path& path, Rules& rules, std::string& error, bool create
) {
    try {
        if (!std::filesystem::exists(path)) {
            if (!create) {
                error = "rules file not found";
                return false;
            }
            // An independent profile can start concurrently; never truncate a file
            // another process/user created between the existence check and this write.
#if defined(_WIN32)
            int fd = -1;
            const auto opened = _wsopen_s(
                &fd,
                path.c_str(),
                _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY,
                _SH_DENYNO,
                _S_IREAD | _S_IWRITE
            );
            if (opened != 0) {
                errno = opened;
            }
#else
            const int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644);
#endif
            if (fd < 0 && errno != EEXIST) {
                error = "could not create rules template";
                return false;
            }
            if (fd >= 0) {
                const auto text = rules_template();
#if defined(_WIN32)
                const auto written = _write(fd, text.data(), static_cast<unsigned>(text.size()));
                const auto closed = _close(fd);
#else
                const auto written = write(fd, text.data(), text.size());
                const auto closed = close(fd);
#endif
                if (written < 0 || static_cast<std::size_t>(written) != text.size()
                    || closed != 0) {
                    error = "could not write rules template";
                    return false;
                }
            }
        }
        std::ifstream in(path, std::ios::binary);
        std::string text(32769, '\0');
        in.read(&text[0], text.size());
        const auto count = in.gcount();
        if (!in.is_open() || in.bad() || count > 32768) {
            error = "could not read rules (32 KiB maximum)";
            return false;
        }
        text.resize(static_cast<std::size_t>(count));
        // UTF-8 BOM is accepted for files saved by Windows editors.
        if (text.rfind("\xEF\xBB\xBF", 0) == 0) {
            text.erase(0, 3);
        }
        return parse_rules(text, rules, error);
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}
}  // namespace weaponxp
