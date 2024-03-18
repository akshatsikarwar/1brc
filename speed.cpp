#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <map>
#include <pthread.h>
#include <string>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <unistd.h>
#include <vector>
struct data {
    int min;
    int max;
    int64_t sum;
    int count;
    data(int t) : min(t), max(t), sum(t), count(1) {}
};
int fd;
static int prev_newline(off_t offset)
{
    char buf[128];
    size_t n = pread(fd, buf, sizeof(buf), offset - sizeof(buf));
    --n;
    while (buf[n] != '\n') --n;
    return offset - (sizeof(buf) - n);
}
struct thd_arg {
    off_t from;
    off_t to;
    int lines;
    std::map<std::string, data> map;
};
static void *thd(void *ptr)
{
    struct thd_arg *arg = (struct thd_arg *)ptr;
    int from = arg->from;
    int to = arg->to;
    int left = to - from + 1;
    int sz = 1 * 1024 * 1024;
    char *buf = (char *)malloc(sz);
    while (left) {
        int n;
        if (left < sz) {
            sz = left;
        }
        n = pread(fd, buf, sz, from);
        while (buf[n - 1] != '\n') {
            --n;
            if (n < 0) abort();
        }
        from += n;
        left -= n;
        char *b = buf;
        while (b < buf + n) {
            ++arg->lines;
            char *sep = strchr(b, ';');
            std::string town(b, sep);
            ++sep; //;
            int bias = 1;
            if (*sep == '-') {
                bias = -1;
                ++sep; //-
            }
            int val = *sep - '0';
            ++sep;
            if (*sep != '.') {
                val *= 10;
                val += (*sep - '0');
                ++sep;
            }
            ++sep; //.
            int frac = *sep - '0';
            int temp = ((val * 10 + frac) * bias);
            b = sep + 2; // new line
            auto t = arg->map.find(town);
            if (t == arg->map.end()) {
                arg->map.insert(std::pair<std::string, data>(town, {temp}));
            } else {
                auto &v = t->second;
                if (v.min > temp) v.min = temp;
                else if (v.max < temp) v.max = temp;
                v.sum += temp;
                v.count += 1;
            }
        }
    }
    free(buf);
    return NULL;
}
int main(int argc, char **argv)
{
    struct timeval time_start, time_end, time_total;
    gettimeofday(&time_start, NULL);
    fd = open(argv[1], O_RDONLY);
    struct stat sb;
    fstat(fd, &sb);
    const int THDS = get_nprocs() * 2;
    const off_t chunk = sb.st_size / THDS;
    pthread_t thds[THDS];
    struct thd_arg args[THDS];
    off_t last = 0;
    for (int i = 0; i < THDS; ++i) {
        args[i].lines = 0;
        args[i].from = last;
        if (i < THDS - 1) {
            last = prev_newline(last + chunk);
        } else {
            last = sb.st_size - 1;
        }
        args[i].to = last++;
        pthread_create(&thds[i], NULL, thd, args + i);
    }
    for (int i = 0; i < THDS; ++i) {
        pthread_join(thds[i], NULL);
    }
    int lines = args[0].lines;
    auto& map = args[0].map;
    for (int i = 1; i < THDS; ++i) {
        lines += args[i].lines;
        for (const auto& town : args[i].map) {
            const auto &found = map.find(town.first);
            if (found == map.end()) {
                map.insert(town);
            } else {
                auto &a = found->second;
                auto &b = town.second;
                if (a.min > b.min) a.min = b.min;
                if (a.max < b.max) a.max = b.max;
                a.sum += b.sum;
                a.count += b.count;
            }
        }
    }
    std::vector<std::pair<std::string, data>> vals(map.begin(), map.end());
    std::sort(vals.begin(), vals.end(), [](const auto &l, const auto &r) { return l.first < r.first; });
    const char *comma = "{";
    for (const auto & v : vals) {
        const auto &d = v.second;
        printf("%s%s=%.1f/%.1f/%.1f", comma, v.first.c_str(), d.min / 10.0, d.sum / (10.0 * d.count), d.max / 10.0);
        comma = ", ";
    }
    puts("}");
    gettimeofday(&time_end, NULL);
    timersub(&time_end, &time_start, &time_total);
    printf("lines:%d time: %lds.%ldms\n", lines, time_total.tv_sec, time_total.tv_usec / 1000);
    close(fd);
    return EXIT_SUCCESS;
}
