#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <numeric>
#include <algorithm>
#include <cstdint>

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

struct mapped_file {
	mapped_file(const char *path)
	: data{nullptr}, size{0} {
		int _fd = open(path, O_RDONLY);
		if (_fd < 0) {
			perror("failed to open file");
			return;
		}

		struct stat st;
		if (fstat(_fd, &st) < 0) {
			perror("failed to stat file");
			return;
		}

		size = st.st_size;

		data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, _fd, 0);
		if (data == MAP_FAILED) {
			data = nullptr;
			perror("failed to mmap file");
			return;
		}

		close(_fd);
	}

	~mapped_file() {
		munmap(data, size);
	}

	void *data;
	size_t size;
};

enum class type {
	allocation,
	deallocation
};

struct alloc_log {
	type t;
	uintptr_t ptr;
	size_t size;
	std::vector<uintptr_t> stack;
};

namespace std {

template <>
struct hash<std::vector<uintptr_t>> {
	size_t operator()(const std::vector<uintptr_t> &vec) const {
		size_t v = vec.size();

		for(auto &i : vec) {
			v ^= i + uintptr_t(0x9e3779b9) + (v << 6) + (v >> 2);
		}

		return v;
	}
};

} // namespace std

int main(int argc, char **argv) {
	if (argc != 3) {
		fprintf(stderr, "usage: <input file> <executable>\n");
		return 1;
	}

	mapped_file in{argv[1]};
	if (!in.data)
		return 1;

	uint8_t *data = static_cast<uint8_t *>(in.data);
	size_t i = 0;

	auto decode_varint = [](uint8_t *buffer, uintptr_t &target) -> size_t {
		target = 0;
		for (int i = 0; i < 8; i++)
			target |= (uintptr_t(buffer[i]) << (i * 8));

		return 8;
	};

	auto print_stack = [](const std::vector<uintptr_t> &stack) {
		for (auto p : stack)
			printf("\t%016lx\n", p);
	};

	std::vector<alloc_log> logs{};
	std::unordered_map<uintptr_t, alloc_log *> unmatched_logs{};
	std::unordered_map<std::vector<uintptr_t>, std::vector<size_t>> grouped_logs{};
	std::vector<std::pair<std::vector<uintptr_t>, std::vector<size_t>>> leaks{};

	while (i < in.size) {
		char mode = data[i++];
		uintptr_t pointer = 0;
		uintptr_t size = 0;
		std::vector<uintptr_t> stack;
		stack.clear();

		i += decode_varint(data + i, pointer);
		if (mode == 'a')
			i += decode_varint(data + i, size);

		uintptr_t tmp = 0;
		while (i < in.size) {
			i += decode_varint(data + i, tmp);
			if (tmp == 0xA5A5A5A5A5A5A5A5)
				break;

			stack.push_back(tmp);
		}

		logs.push_back({mode == 'a' ? type::allocation : type::deallocation, pointer, size, stack});
	}

	for (auto &l : logs) {
		if (l.t == type::allocation) {
			if (unmatched_logs.count(l.ptr)) {
				printf("same address allocated again without matching free for previous call?\n");
				printf("address %016lx got allocated again despite not being freed!\n", l.ptr);
				printf("first allocation from:\n");
				print_stack(unmatched_logs[l.ptr]->stack);
				printf("allocation again from:\n");
				print_stack(l.stack);
			} else {
				unmatched_logs[l.ptr] = &l;
			}
		} else {
			if (unmatched_logs.count(l.ptr)) {
				unmatched_logs.erase(l.ptr);
			} else if (l.ptr) {
				printf("deallocation of an address that wasn't allocated?\n");
				printf("address %016lx isn't allocated anywhere at this point!\n", l.ptr);
				printf("deallocated from:\n");
				print_stack(l.stack);
			}
		}
	}

	for (auto &[ptr, l] : unmatched_logs) {
		if (grouped_logs.count(l->stack)) {
			grouped_logs[l->stack].push_back(l->size);
		} else {
			grouped_logs[l->stack] = {l->size};
		}
	}

	for (auto &[stack, l] : grouped_logs) {
		leaks.push_back(std::make_pair(stack, l));
	}

	std::sort(leaks.begin(), leaks.end(),
		[](auto &a, auto &b){
			return std::accumulate(a.second.begin(), a.second.end(), 0)
				< std::accumulate(b.second.begin(), b.second.end(), 0);
		}
	);

	int stdin_pipe[2];
	int stdout_pipe[2];
	pipe(stdin_pipe);
	pipe(stdout_pipe);

	// setup addr2line things
	pid_t addr2line_pid = fork();
	if (addr2line_pid < 0) {
		perror("fork failed");
		return 1;
	}

	if (!addr2line_pid) {
		dup2(stdin_pipe[0], STDIN_FILENO);
		dup2(stdout_pipe[1], STDOUT_FILENO);
		execl("/usr/bin/addr2line", "addr2line", "-Cpfse", argv[2], nullptr);
	}

	// write to stdin_pipe[1], read from stdout_pipe[0]
	FILE *stdin_f = fdopen(stdin_pipe[1], "w"), *stdout_f = fdopen(stdout_pipe[0], "r");

	if (!stdin_f) {
		perror("failed to open stdin pipe");
		return 1;
	}

	if (!stdout_f) {
		perror("failed to open stdout pipe");
		return 1;
	}

	size_t total_all = 0;

	char *linebuf = nullptr;
	size_t linecap = 0;

	for (auto &[stack, l] : leaks) {
		size_t avg = std::accumulate(l.begin(), l.end(), 0) / l.size();
		size_t total = std::accumulate(l.begin(), l.end(), 0);
		total_all += total;
		printf("%lu potential leak(s) found of average size %lu, total size %lu, and all sizes:\n  ", l.size(), avg, total);

		std::sort(l.begin(), l.end());

		size_t i = 0;

		while (i < l.size()) {
			size_t n = std::count(l.begin(), l.end(), l[i]);

			if (n == 1)
				printf("%lu", l[i]);
			else
				printf("%lux %lu", n, l[i]);

			i += n;
			printf("%s", i < l.size() ? ", " : "");
		}

		printf("\n  found in:\n");
		bool top = true;
		for (auto p : stack) {
			// For stack frames below the top, subtract 1 to resolve the call instruction
			// and not the next instruction after the call.
			fprintf(stdin_f, "0x%016lx\n", (!p || top) ? p : (p - 1));
			fflush(stdin_f);
			getline(&linebuf, &linecap, stdout_f);

			printf("\t%016lx -> %s", p, linebuf);
			top = false;
		}
		printf("--------------------------------------\n\n");
	}

	free(linebuf);
	fclose(stdin_f);
	fclose(stdout_f);
	close(stdin_pipe[1]);
	close(stdout_pipe[0]);

	printf("total potential leaks: %lu, which is %lu bytes\n", unmatched_logs.size(), total_all);

	kill(addr2line_pid, SIGTERM);
	int wstatus;
	waitpid(addr2line_pid, &wstatus, 0);
}
