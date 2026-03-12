int sum(int end) {
	int ans = end;
	for (int i = 1; i < end; i++)
		ans += i;

	return ans;
}
