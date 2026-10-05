// Copyright (C) 2026 Church Director contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <iostream>

namespace {
bool expect(bool condition, const char *message)
{
	if (!condition) {
		std::cerr << "FAIL: " << message << '\n';
		return false;
	}
	return true;
}
} // namespace

int main()
{
	if (!expect(true, "bootstrap_test_executable_runs"))
		return EXIT_FAILURE;
	std::cout << "PASS: bootstrap_test_executable_runs\n";
	return EXIT_SUCCESS;
}
