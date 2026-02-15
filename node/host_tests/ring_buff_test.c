#include <stdio.h>
#include <assert.h>

#include "../main/ring_buf.h"

static ring_buf_t make_entry(int id);

void test_underflow( void);
void test_fifo (int num_entries);
void test_overflow (int num_entries);
void test_wrap (int num_entries);
void test_reset (int num_entries);
void test_interleaved (int num_entries);
void test_double_overflow (int num_entries);
void test_count (int num_entries);


int main(void) {
    test_underflow();

    for (int i=1; i<=RING_BUFFER_CAP; i++) {
        test_fifo(i);
        test_reset(i);
    }

    for (int i=20; i<=RING_BUFFER_CAP; i++) {
        test_overflow(i);
        test_double_overflow(i);
        test_wrap(i);
        test_interleaved(i);
        test_count(i);
    }

    printf("\033[32mAll tests passed!\033[0m\n");
    return 0;
}

static ring_buf_t make_entry(int id) {
    ring_buf_t temp = {0};
    temp.task_id = id;
    temp.hits    = id*75; // 75 is random, just for testing
    temp.total   = 10000;
    temp.compute_ms = 300;
    return temp;
}

void test_underflow( void) {
    ring_buf_t pop;
    rbuf_reset();
    assert(rbuf_count() == 0);
    assert(rbuf_pop(&pop) == false);
}

void test_single_push_pop (void) {
    ring_buf_t push = make_entry(50);
    ring_buf_t pop;

    rbuf_reset();
    rbuf_push(&push);
    rbuf_pop(&pop);
    assert(pop.task_id == 50);
    assert(pop.hits == 50 * 75);
}

void test_fifo (int num_entries) {
    ring_buf_t push;
    ring_buf_t pop;

    rbuf_reset();

    for (int i=0; i<num_entries; i++) {
        push = make_entry(i);
        rbuf_push(&push);
    }

    for (int i=0; i<num_entries; i++) {
        rbuf_pop(&pop);
        assert(pop.task_id == i);
    }
}

void test_overflow (int num_entries) {
    ring_buf_t push;
    ring_buf_t pop;

    rbuf_reset();

    for (int i=0; i<RING_BUFFER_CAP+num_entries; i++) {
        push = make_entry(i);
        rbuf_push(&push);
    }

    assert(rbuf_count() == RING_BUFFER_CAP);

    for (int i=num_entries; i<RING_BUFFER_CAP+num_entries; i++) {
        rbuf_pop(&pop);
        assert(pop.task_id == i);
    }
}

void test_double_overflow (int num_entries) {
    ring_buf_t push;
    ring_buf_t pop;

    rbuf_reset();

    for (int i=0; i<RING_BUFFER_CAP+num_entries; i++) {
        push = make_entry(i);
        rbuf_push(&push);
    }

    for (int i=0; i<5; i++) {
        rbuf_pop(&pop);
        assert(pop.task_id == num_entries + i);
    }

    for (int i=0; i<RING_BUFFER_CAP+num_entries; i++) {
        push = make_entry(i+100);
        rbuf_push(&push);
    }

    assert(rbuf_count() == RING_BUFFER_CAP);

    for (int i=100+num_entries; i<100+num_entries+RING_BUFFER_CAP; i++) {
        rbuf_pop(&pop);
        assert(pop.task_id == i);
    }
}

void test_wrap (int num_entries) {
    ring_buf_t push;
    ring_buf_t pop;

    rbuf_reset();

    for (int i=0; i<num_entries; i++) {
        push = make_entry(i);
        rbuf_push(&push);
    }

    for (int i=0; i<num_entries-5; i++) {
        rbuf_pop(&pop);
        assert(pop.task_id == i);
    }

    for (int i=0; i<RING_BUFFER_CAP+num_entries; i++) {
        push = make_entry(i+100);
        rbuf_push(&push);
    }

    for (int i=100+num_entries; i<100+num_entries+RING_BUFFER_CAP; i++) {
        rbuf_pop(&pop);
        assert(pop.task_id == i);
    }
}

void test_reset (int num_entries) {
    ring_buf_t push;
    ring_buf_t pop;

    rbuf_reset();

    for (int i=0; i<num_entries; i++) {
        push = make_entry(i);
        rbuf_push(&push);
    }

    rbuf_reset();
    assert(rbuf_count() == 0);
    assert(rbuf_pop(&pop) == false);
}

void test_interleaved (int num_entries) {
    ring_buf_t push;
    ring_buf_t pop;
    int next_pop = 0;

    rbuf_reset();

    for (int i=0; i<num_entries; i++) {
        push = make_entry(i);
        rbuf_push(&push);
        if (i % 5 == 0) {
            rbuf_pop(&pop);
            assert(pop.task_id == next_pop);
            next_pop++;
        }
    }

    while (rbuf_pop(&pop)) {
        assert(pop.task_id == next_pop);
        next_pop++;
    }
}

void test_count (int num_entries) {
    ring_buf_t push;
    ring_buf_t pop;

    rbuf_reset();

    for (int i=0; i<num_entries; i++) {
        push = make_entry(i);
        rbuf_push(&push);
        assert(rbuf_count() == (i + 1 < RING_BUFFER_CAP ? i + 1 : RING_BUFFER_CAP)); // +1 because count is checked after push
    }

    for (int i=0; i<5; i++) {
        rbuf_pop(&pop);
        assert(rbuf_count() == num_entries - i - 1);
    }

    for (int i=0; i<RING_BUFFER_CAP+num_entries; i++) {
        push = make_entry(i);
        rbuf_push(&push);
        assert(rbuf_count() == (num_entries - 5 + i + 1 < RING_BUFFER_CAP ? num_entries - 5 + i + 1 : RING_BUFFER_CAP));
    }
}