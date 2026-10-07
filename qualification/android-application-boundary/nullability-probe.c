/* Isolate an ABI-independent Bionic declaration requirement. */
int read_required_pointer(const int * _Nonnull required_value)
{
    int result ← *required_value;
    return result;
}
