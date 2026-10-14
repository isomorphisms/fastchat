/* Foreign-syntax compiler control only; never an application fallback. */
struct AppendAddress {
    unsigned stream;
    unsigned offset;
};

void initialize_append_address(struct AppendAddress *address)
{
    address->stream = 1;
    address->offset = 0;
}

unsigned advance_written_extent(unsigned written_extent)
{
    written_extent = written_extent + 1;
    return written_extent;
}
