#pragma once
#include "tester_utils.h"
#include "../mdump/mdump.h"
#include "../lib/jot/defines.h"

namespace jot::tests
{


    /*
    struct MockConstruct 
    {
        struct {
            i64 id;
            i64 size;
        } header_;

        i64 index_;
        i64 depth_;
        i64 payload_;
        ReadContext* context = nullptr;

        static proc abs_(i64 num)
        {
            if(num > 0)
                return num;
            else
                return -num;
        };

        proc id()       const {return (IdBase)abs_(header_.id);}
        proc size()     const {return (SizeBase)abs_(header_.size);}
        proc index()    const {return (u64)abs_(index_);}
        proc depth()    const {return (u64)abs_(depth_);}
        proc payload()  const  {return (u64)abs_(payload_);}

        proc header()   const {return ObjectHeader{id(), size()};}
    };

    proc put_mock_data(let* constructs, mut construct_count, mut& outputer, let& context)
    {
        for(decltype(construct_count) i = 0; i < construct_count; i++)
        {
            let& curr = constructs[i];
            let* curr_context = &context;
            if(curr.context != nullptr)
                curr_context = curr.context;
        
            if(put_header(curr.header(), outputer, *curr_context) == false)
                return false;
            if(is_list_id(curr.id()) == false)
                if(put_integer(curr.payload(), outputer, curr.size(), curr_context->settings.endian) != HandleState::Success)
                    return false;
        }

        return true;
    }

    proc check_read_mock_data(let& header, let& scope, let& construct, mut& inputer, let& context)
    {
        #define return_error(expr) if((expr) == false) return false

        if(construct.header_.id >= 0)
            return_error(header.id == construct.id());
        
        if(construct.header_.size >= 0)
            return_error(header.size == construct.size());

        if(is_list_id(construct.id()) == false)
        {
            u64 payload = 0;
            return_error(get_integer(payload, inputer, construct.size(), context.settings.endian) == HandleState::Success);
        
            if(construct.payload_ >= 0)
                return_error(payload == construct.payload());
        }

        if(construct.index_ >= 0)
            return_error(scope.index == construct.index());
    
        if(construct.depth_ >= 0)
            return_error(scope.depth == construct.depth());

        #undef return_error
        return true;
    }

    TEST_CASE("[interpret_unhandled] should recursively iterate over all all bytes from inputer given state", "[mindump]") 
    {
        byte input_data[400];
        ByteInputer inputer = {{input_data}};
        ByteOutputer outputer = {{input_data}};

        ReadState state;
    
        SECTION("when provided with single plain object it should be interpreted and then return true; no other use lambdas should be called")
        {
            let& context = state.context();
            let given_header = ObjectHeader{20, 8};
            let given_payload = static_cast<u64>(0x1122334455);
            put_header(given_header, outputer, context);
            put_integer(given_payload, outputer, given_header.size, context.settings.endian);

            let ret = interpret_unhandled(inputer, state, 
                [&](mut& header, mut& scope, mut& context, mut& new_context) {
                    mut payload = static_cast<u64>(0);
                    get_integer(payload, inputer, header.size, context.settings.endian);
                    REQUIRE(payload == given_payload);
                
                    REQUIRE(header.id == given_header.id);
                    REQUIRE(header.size == given_header.size);

                    REQUIRE(scope.depth == 0);
                    REQUIRE(scope.index == 0);
                    return true;
                },
                [&](mut&, mut&, mut&, mut&) {REQUIRE(false); return false;},
                [&](mut&, mut&, mut&, mut&) {REQUIRE(false); return false;});

            REQUIRE(ret == true);
        }
    
        SECTION("when provided with empty list object it should be interpreted and then return true; only begin and end use lambdas should be called")
        {
            let& context = state.context();
            let given_header = ObjectHeader{21, Size::Auto};
            put_header(given_header, outputer, context);
            put_header(given_header, outputer, context);

            mut end_count = 0;
            mut begin_count = 0;
            let ret = interpret_unhandled(inputer, state, 
                [&](mut&, mut&, mut&, mut&) {
                    REQUIRE(false); return false;
                },
                [&](mut& header, mut& scope, mut& context, mut& new_context) {

                    REQUIRE(header.id == given_header.id);
                    REQUIRE(header.size == given_header.size);

                    REQUIRE(scope.depth == 0);
                    REQUIRE(scope.index == 0);
                
                    begin_count ++;
                    return true;
                },
                [&](mut& header, mut& scope, mut& context, mut& new_context) {
                    REQUIRE(header.id == given_header.id);
                    REQUIRE(header.size == given_header.size);

                    REQUIRE(scope.depth == 1);
                    REQUIRE(scope.index == 0);
                
                    end_count ++;
                    return true;
                });

            REQUIRE(ret == true);
            REQUIRE(end_count == 1);
            REQUIRE(begin_count == 1);
        }

        constexpr u64 LIST1 = 21;
        constexpr u64 LIST2 = 23;
        constexpr u64 LIST3 = 81;
        constexpr u64 LIST4 = 25;
        constexpr u64 LIST5 = 27;
        constexpr u64 OBJ = 38;
        constexpr i64 DONT_CHECK = -1;
        constexpr let AUTO = Size::Auto;

        let checker = [&](MockConstruct given[], size_t given_count){
        
            mut index = 0;
            let compare_mock = [&](mut& header, mut& scope, mut& context, mut& new_context){
                let ret = check_read_mock_data(header, scope, given[index], inputer, context);
                REQUIRE(ret);

                index++;
                return true;
            };
            let blank_use = [](mut&, mut&, mut&, mut&){
                return true;
            };

            let& context = state.context();
            put_mock_data(given, given_count, outputer, context);

            let ret = interpret_unhandled(inputer, state, 
                compare_mock,
                compare_mock,
                blank_use,
                compare_mock
            );

            REQUIRE(ret == true);
        };

        SECTION("when provided with auto sized list with multiple objects it should be interpreted as expected")
        {
            MockConstruct given[] = {
                {{LIST1, AUTO}, (0), (0)},
                    {{OBJ, 7}, (0), (1), {0x1122334455}},
                    {{OBJ, 4}, (1), (1), {0x634646}},
                    {{OBJ, 2}, (2), (1), {0x8899}},
                    {{LIST1, AUTO}, (3), (1)}
            };

            constexpr size_t given_count = sizeof(given) / sizeof(MockConstruct);
            checker(given, given_count);
        }
    

        SECTION("when provided with nested auto sized lists with multiple objects it should be interpreted as expected")
        {
            MockConstruct given[] = {
                {{LIST1, AUTO}, (0), (0)},
                    {{OBJ, 7}, (0), (1), {0x1122334455}},
                    {{OBJ, 4}, (1), (1), {0x634646}},
                    {{OBJ, 2}, (2), (1), {0x8899}},
                
                    {{LIST2, AUTO}, (3), (1)},
                        {{OBJ, 7}, (0), (2), {0x1122334455}},
                        {{OBJ, 4}, (1), (2), {0x634646}},
                        {{OBJ, 2}, (2), (2), {0x8899}},

                        {{LIST3, AUTO}, (3), (2)},
                            {{OBJ, 7}, (0), (3), {0x1122334455}},
                            {{OBJ, 4}, (1), (3), {0x634646}},
                            {{OBJ, 2}, (2), (3), {0x8899}},
                            {{LIST3, AUTO}, (3), (3)},
                    
                        {{OBJ, 7}, (4), (2), {0x1122334455}},
                        {{OBJ, 4}, (5), (2), {0x634646}},
                        {{OBJ, 2}, (6), (2), {0x8899}},
                        {{LIST2, AUTO}, (7), (2)},

                    {{LIST1, AUTO}, (4), (1)}
            };
            constexpr size_t given_count = sizeof(given) / sizeof(MockConstruct);
            checker(given, given_count);
        }

        SECTION("when provided with combination of auto sized and static sized lists it should behave as expected")
        {
            MockConstruct given[] = {
                {{LIST1, AUTO}, (0), (0)}, //0
                    {{OBJ, 8},   (0), (1), {0x1122334455}}, //1
                    {{LIST2, 2}, (1), (1)}, //2
                        {{OBJ, 4}, (0), (2), {0x112233}}, //3
                        {{OBJ, 2}, (1), (2), {0x8899}}, //4

                    {{OBJ, 8},   (2), (1), {0x1122334455}}, //5
                    {{LIST3, AUTO}, (3), (1)}, //6
                        {{LIST4, 3}, (0), (2)}, //7
                            {{OBJ, 7}, (0), (3), {0x1122334455}}, //8
                            {{OBJ, 4}, (1), (3), {0x634646}}, //9
                            {{OBJ, 2}, (2), (3), {0x8899}}, //10
                        {{OBJ, 2}, (1), (2), {0x8899}}, //11
                        {{LIST3, AUTO}, (2), (2)}, //12
                    
                    {{LIST5, 1}, (4), (1)}, //13
                        {{OBJ, 5}, (0), (2), {0x56414}}, //14
                    {{LIST1, AUTO}, (5), (1)},  //15
            };
        
            constexpr size_t given_count = sizeof(given) / sizeof(MockConstruct);
            checker(given, given_count);
        }
    }
    */
}

#include "../lib/jot/undefs.h"