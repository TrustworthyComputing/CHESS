// min index of an array of size 10 and int type.
void min_index(int arr[10], int result[10]) {
    int min_value = arr[0];
    for (int i = 1; i < 10; ++i) {
        if (arr[i] < min_value) {
            min_value = arr[i];
        }
    }

    for (int i = 0; i < 10; ++i) {
        result[i] = (arr[i] == min_value) ? 1 : 0;
    }
}
