int main(unsigned hart_id) {
	// behave differently based on the core (i.e. hart) id
	for (unsigned i = 0; i < 100 * hart_id; ++i)
		;
	return 0;
}
