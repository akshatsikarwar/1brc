#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>
struct data {
    float min;
    float max;
    float sum;
    int count;
    data(float t) : min(t), max(t), sum(t), count(1) {}
};
int main(int argc, char **argv)
{
    struct timeval time_start, time_end, time_total;
    gettimeofday(&time_start, NULL);
    std::map<std::string, data> map;
    int fd = open(argv[1], O_RDONLY);
    char buf[4 * 1024 * 1024];
    int towns = 0, lines = 0;
    off_t from = 0;
    ssize_t n;
    while ((n = pread(fd, buf, sizeof(buf), from)) > 0) {
        while (buf[n - 1] != '\n') --n;
        from += n;
        char *b = buf;
        while (b < buf + n) {
            ++lines;
            char *sep = strstr(b, ";");
            std::string town(b, sep);
            char *nl;
            float temp = strtof(sep + 1, &nl);
            b = nl + 1;
            auto t = map.find(town);
            if (t == map.end()) {
                ++towns;
                map.insert(std::pair<std::string, data>(town, {temp}));
            } else {
                auto &v = t->second;
                if (v.min > temp) v.min = temp;
                else if (v.max < temp) v.max = temp;
                v.sum += temp;
                v.count += 1;
            }
        }
    }
    std::vector<std::pair<std::string, data>> vals(map.begin(), map.end());
    std::sort(
        vals.begin(),
        vals.end(),
        [](const auto &l, const auto &r) {
            return l.first < r.first;
        }
    );
    const char *comma = "{";
    for (const auto & v : vals) {
        const auto &d = v.second;
        printf("%s%s=%.1f/%.1f/%.1f", comma, v.first.c_str(), d.min, d.sum / d.count, d.max);
        comma = ", ";
    }
    puts("}");
    gettimeofday(&time_end, NULL);
    timersub(&time_end, &time_start, &time_total);
    printf("lines:%d towns:%d time:%lds.%ldms\n", lines, towns, time_total.tv_sec, time_total.tv_usec / 1000);
    close(fd);
    return EXIT_SUCCESS;
}
