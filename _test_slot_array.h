#pragma once

#include "_test.h"
#include "slot_array.h"

namespace jot::tests::slot_array
{
    template <typename T>
    void print_slot_array(Slot_Array<T> const& array);
    
    template <typename T>
    static void test_insert_remove_get(Static_Array<T, 10> elems)
    {
        isize trackers_before = trackers_alive();
        isize memory_before = default_allocator()->get_stats().bytes_allocated;
        {
            Slot_Array<T> array;
            TEST(size(array) == 0);
            TEST(capacity(array) == 0);

            Slot slot0 = insert(&array, dup(elems[0]));
            Slot slot1 = insert(&array, dup(elems[1]));
            Slot slot2 = insert(&array, dup(elems[2]));

            TEST(size(array) == 3);
            TEST(capacity(array) >= 3);

            TEST(get(array, slot0) == elems[0]);
            TEST(get(array, slot1) == elems[1]);
            TEST(get(array, slot2) == elems[2]);
        
            TEST(remove(&array, slot1) == elems[1]);
        
            TEST(size(array) == 2);
            TEST(capacity(array) >= 3);
            TEST(get(array, slot0) == elems[0]);
            TEST(get(array, slot2) == elems[2]);

            slot1 = insert(&array, dup(elems[9]));
            TEST(size(array) == 3);
            TEST(capacity(array) >= 3);
            TEST(get(array, slot1) == elems[9]);
        
            Slot slot3 = insert(&array, dup(elems[3]));
            Slot slot4 = insert(&array, dup(elems[4]));
            Slot slot5 = insert(&array, dup(elems[5]));
            Slot slot6 = insert(&array, dup(elems[6]));
            Slot slot7 = insert(&array, dup(elems[7]));
            Slot slot8 = insert(&array, dup(elems[8]));
        
            TEST(size(array) == 9);
            TEST(capacity(array) >= 9);
        
            TEST(get(array, slot0) == elems[0]);
            TEST(get(array, slot1) == elems[9]);
            TEST(get(array, slot2) == elems[2]);
            TEST(get(array, slot3) == elems[3]);
            TEST(get(array, slot4) == elems[4]);
            TEST(get(array, slot5) == elems[5]);
            TEST(get(array, slot6) == elems[6]);
            TEST(get(array, slot7) == elems[7]);
            TEST(get(array, slot8) == elems[8]);
        
            TEST(remove(&array, slot8) == elems[8]);
            TEST(remove(&array, slot6) == elems[6]);
            TEST(remove(&array, slot4) == elems[4]);
        
            TEST(size(array) == 6);
            TEST(capacity(array) >= 9);
            TEST(get(array, slot0) == elems[0]);
            TEST(get(array, slot7) == elems[7]);
            TEST(get(array, slot5) == elems[5]);
        
            TEST(remove(&array, slot0) == elems[0]);
            TEST(remove(&array, slot1) == elems[9]);
            TEST(remove(&array, slot2) == elems[2]);
            TEST(remove(&array, slot3) == elems[3]);
            TEST(remove(&array, slot5) == elems[5]);
            TEST(remove(&array, slot7) == elems[7]);
        
            TEST(size(array) == 0);
            TEST(capacity(array) >= 9);

            slot5 = insert(&array, dup(elems[5]));
            TEST(get(array, slot5) == elems[5]);
        }
        isize trackers_after = trackers_alive();
        isize memory_after = default_allocator()->get_stats().bytes_allocated;

        TEST(trackers_before == trackers_after);
        TEST(memory_before == memory_after);
    }
    
    static
    void stress_test(bool print = true)
    {
        using Seed = std::random_device::result_type;

        if(print) println("  test_stress()");

        std::random_device rd;
        
        constexpr isize OP_INSERT = 0;
        constexpr isize OP_REMOVE = 1;

        std::discrete_distribution<unsigned> op_distribution({75, 25});
        std::uniform_int_distribution<unsigned> index_distribution(0);

        std::mt19937 gen;
        const auto test_batch = [&](isize block_size, isize j){
            i64 trackers_before = trackers_alive();
            isize memory_before = default_allocator()->get_stats().bytes_allocated;

            {
                Hash_Table<isize, Slot, int_hash<isize>> truth;
                Slot_Array<isize> slot_array;

                reserve(&truth, block_size);

                for(isize i = 0; i < block_size; i++)
                {
                    isize op = (isize) op_distribution(gen);
                    
                    bool skipped = false;
                    switch(op)
                    {
                        case OP_INSERT: {
                            Slot slot = insert(&slot_array, i);
                            set(&truth, i, slot);
                            break;
                        }

                        case OP_REMOVE: {
                            Slice<const isize> truth_vals = keys(truth);
                            Slice<Slot> truth_indices = values(&truth);
                            if(truth_indices.size == 0)
                            {   
                                skipped = true;
                                break;
                            }

                            isize selected_index = (isize) index_distribution(gen) % truth_indices.size;
                            Slot removed_slot = truth_indices[selected_index];
                            isize removed_value = truth_vals[selected_index];

                            isize just_removed = remove(&slot_array, removed_slot);
                            TEST(just_removed == removed_value);
                            remove(&truth, removed_value);
                            break;
                        }

                        default: break;
                    }
                    
                    if(skipped)
                    {
                        i--;
                        continue;
                    }

                    Slice<const isize> truth_vals = keys(truth);
                    Slice<Slot> truth_indices = values(&truth);
                    isize used_size = size(slot_array);
                    TEST(used_size == truth_vals.size);

                    for(isize k = 0; k < truth_indices.size; k++)
                    {
                        Slot slot = truth_indices[k];
                        isize retrieved = get(slot_array, slot);
                        TEST(retrieved == truth_vals[k]);
                    }
                }
            
                if(print) println("    i: {}\t batch: {}\t final_size: {}", j, block_size, size(slot_array));
            }

            i64 trackers_after = trackers_alive();
            isize memory_after = default_allocator()->get_stats().bytes_allocated;
            TEST(trackers_before == trackers_after);
            TEST(memory_before == memory_after);
        };
        
        Seed seed = rd();
        gen = std::mt19937(seed);
        for(isize i = 0; i < 10; i++)
        {
            test_batch(10, i);
            test_batch(10, i);
            test_batch(40, i);
            test_batch(160, i);
            test_batch(640, i);
            test_batch(640*4, i);
            test_batch(640*16, i);
        }
    }
    

    static void test_slot_array(u32 flags)
    {
        bool print = !(flags & Test_Flags::SILENT);

        Static_Array<i32, 10>          arr1 = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        Static_Array<Test_String, 10>  arr2 = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10"};
        Static_Array<Tracker<i32>, 10> arr3 = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        
        if(print) println("\ntest_slot_array()");
        if(print) println("  type: i32");
        test_insert_remove_get(arr1);

        if(print) println("  type: Test_String");
        test_insert_remove_get(arr2);
        
        if(print) println("  type: Tracker<i32>");
        test_insert_remove_get(arr3);
        
        if(flags & Test_Flags::STRESS)
            stress_test(print);
    }
}

namespace jot::tests::slot_array
{
    template <typename T>
    void print_slot_array(Slot_Array<T> const& array)
    {
        using namespace slot_array_internal;
        auto print_esc_at = [&](isize i, isize offset){
            if(i != 0) print(", ");

            isize curr = array._slots[i*SLOT_SIZE + offset];
            if(curr == (uint32_t) -1)
                print(" .");
            else
                print(Padded_Int_Format{curr, 2, ' '});
        };  

        println("{");
        println("    size:        {}", array._size);
        println("    capacity:    {}", array._capacity);

        print  ("                  ");
        for(isize i = 0; i < array._capacity; i++)
        {
            if(i != 0) print(", ");
            print(Padded_Int_Format{i, 2, ' '});
        }
        println();

        print  ("    items:       [");
        for(isize i = 0; i < array._capacity; i++)
        {
            if(i != 0)
                print(", ");

            uint32_t index = array._slots[i*SLOT_SIZE + ITEM];
            if(index == (uint32_t) -1)
                print("__");
            else
                print(Padded_Int_Format{array._data[index], 2, ' '});

        }
        print("]\n");
        
        print  ("    slot items:  [");
        for(isize i = 0; i < array._capacity; i++)
            print_esc_at(i, ITEM);
        print("]\n");
        
        print  ("    slot owners: [");
        for(isize i = 0; i < array._capacity; i++)
            print_esc_at(i, OWNER);
        print("]\n");
        
        print  ("    slot nexts:  [");
        for(isize i = 0; i < array._capacity; i++)
            print_esc_at(i, NEXT);
        print("]\n");

        print  ("    free list:   ");
        uint32_t curr = array._free_list;
        while(curr != -1)
        {
            print(curr);
            //print_slot(array._slots + curr*SLOT_SIZE);
            print(" -> ");
            curr = array._slots[curr*SLOT_SIZE + NEXT];
        }
        print("-1\n");
        println("}");
    }

}