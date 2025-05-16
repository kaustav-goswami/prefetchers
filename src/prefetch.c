#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

// Implementation in C 

// The paper defines a length of the window. By default, we use 64
#define LENGTH 16
// We need to define a packet size. For DRAM systems, it has be the same as a
// cache line size. This is in BYTES!
#define PKT_SIZE 16

// For ladder traffic, there is a stride for a couple of addresses before going
// back to an random-like pattern. The stride pattern continues until
// LADDER_LENGTH - 1 packets and then it drops before rising.
#define LADDER_LENGTH 4
// The ladder can have different lengths too!

// The pattern drops. See the graph from the paper. this is in bytes
#define LADDER_DROP_SIZE 8
// The pattern resumes after a certain interval.
#define LADDER_INTERVAL 1
// Ladder also needs to have the rise. After 10 packets, the 11th packet will
// see this jump. this is in bytes.
#define LADDER_RISE 13

// To generate traffic, we need a base address to start with. When these
// addresses are fed into a memory simulator, the address decoding can be seen
// in details.
#define BASE_ADDR 0x0

// We need to define the prefetcher depth
#define PREFETCHER_DEPTH 1

// We need a global structure that maintains the ladder traffic index
// generators
int processed_ladder = BASE_ADDR;

// Here is maintain the stride related parameters
int access_history[LENGTH];
int stride_history[LENGTH];

// A prefetcher algorithm returns a stride pattern
// The prefetcher struct stores a valid bit to indicate if the results are
// valid or not.
// As per the ladder and ripple algorithms, it returns a stride_target and
// pattern_target.
// This should be oblivious to the prefetcher engine as it should only receive
// a target address or a set of target addresses to prefetch depending on the
// depth of the prefetcher engine.
struct prefetcher {
    bool valid;
    int stride_target;
    int pattern_target;
};

// Utility function
void fatal(char *reason) {
    printf("%s\n", reason);
    exit(-1);
}

// Now we need to define a linear address stream.
int create_linear(int index) {
    /* this function generates linear traffic by adding index * packet_size
     *
     * :param index: nth number of the packet to generate an address for
     */
    return BASE_ADDR + (index * PKT_SIZE);
}

int create_ladder(int index) {
    // increment the ladder
    // current_ladder_traffic++;
    // find the ladder index for the current iteration
    int this_ladder_index = index % LADDER_LENGTH;
    // find the amount of data processed by the traffic generator.
   
    // until LADDER_LENGTH - 1, there will be a const stride. Not necessarily!
    if (this_ladder_index == 0) {
        // this is a rise
        processed_ladder += LADDER_RISE;
    }
    else if ((this_ladder_index > 0) && (this_ladder_index < (LADDER_LENGTH - 1))) {
        processed_ladder += (PKT_SIZE + this_ladder_index);
    }
    // If this is drop
    else if (this_ladder_index == (LADDER_LENGTH - 1)) {
        processed_ladder -= LADDER_DROP_SIZE;
    }
    else {
        // this should not happen
        fatal("error! unhandled exception at create_ladder(..)");
    }

    return processed_ladder;
}

// Can't even use string in C! This is a nightmare.
int generate_traffic(int index, char *traffic_type, bool verbose) {
    /* this function generates a given traffic (linear, dom, ladder, ripple,
     * random). It requires an id for a given traffic packet, i.e., 
     * 
     * :param index: index of the current packet to generate
     * :param traffic_type: must be linear, dom, ladder, ripple, random
     */
   
    int current_traffic;
    if (strcmp(traffic_type, "linear") == 0)
        current_traffic = create_linear(index);
    else if (strcmp(traffic_type, "ladder") == 0)
        current_traffic = create_ladder(index);
    else
        fatal("NotImplementedError! In generate_traffic(..)");
    
    if (verbose == true && false)
        printf("gen: index = %d; addr = %d\n", index, current_traffic);

    return current_traffic;
}

// get the most dominant stride
int most_dominating_stride() {
    // This is incorrect!
    /* AI generated utility function. Was to lazy to write this */
    int maxCount = 0;
    int mostFrequent = stride_history[0];

    for (int i = 0; i < LENGTH - 1; i++) {
        int count = 0;
        for (int j = 0; j < LENGTH; j++) {
            if (stride_history[j] == stride_history[i]) {
                count++;
            }
        }
        if (count > maxCount) {
            maxCount = count;
            mostFrequent = stride_history[i];
        }
    }
    // Stride history is counted in LENGTH - 1 size
    if (maxCount > ((LENGTH - 1) / 2))
        return mostFrequent;
    else
        // There is no domaning stride!
        return -1;
}

int find_most_common_element(int *array, int size) {
    // make sure that the size is not 0;.
    assert(size != 0);

    int max_element = array[0];
    int index = 0;

    for (int i = 0 ; i < size ; i++) {
        if (max_element < array[i]) {
            max_element = array[i];
            index = i;
        }
    }

    return max_element;
}

// begin writing the prefetchers here
struct prefetcher _dominator() {
    /* Calculates the dominant stride in the given access history */
    int dom_stride = most_dominating_stride();
    struct prefetcher output;
    output.stride_target = dom_stride;
    return output;
}

struct prefetcher _ladder(int vpn_a, int stride_a, int pid_a, int *access_history, int *stride_history) {
    /* Implementation based on the HoPP paper. The algo written in the paper
     * was fed into an AI to generate a python version of the algorithm. It was
     * then translated to C.
     *
     * :param vpn_a: Virtual Page Number of the current access must be the one
     *              at the end of the window?? super confusing.
     * :param stride_a: Stride of the current access i.e. vpn_a - last_vpn_a.
     * :param pid_a: The process ID of the current process (ignored).
     * :param *vpn_history: The array of prior vpn history
     * :param *stride_history: The array of prior strides
     * 
     * @returns :struct prefetcher output:
     */
    struct prefetcher output;

    // calculate stride_A
    int pattern_target[2];
    pattern_target[0] = stride_history[LENGTH - 2];
    pattern_target[1] = stride_a;

    // to store the next candidates
    int next_stride[LENGTH];
    int stride_sum[LENGTH];

    // These structures are dynamically filled up!
    int next_stride_index = 0;
    int stride_sum_index = 0;

    // ¯\_(ツ)_/¯
    int last_index = LENGTH - 2;

    for (int i = LENGTH - 3 ; i >= 0 ; i--) {
        if (stride_history[i] == pattern_target[0] && stride_history[i + 1] == pattern_target[1]) {
            next_stride[next_stride_index++] = stride_history[i + 2];
            stride_sum[stride_sum_index++] = access_history[last_index] - access_history[i];
            last_index = i;
        }
    }

    if (next_stride_index > 0) {
        // There is some element in the next_stride array
        output.stride_target = find_most_common_element(next_stride, next_stride_index);
        output.pattern_target = find_most_common_element(stride_sum, stride_sum_index);
    }
    else {
        // return zeros pretty much!
        output.stride_target = 0;
        output.pattern_target = 0;
    }

    return output;

}

struct prefetcher _ripple(int vpn_a, int stride_a, int pid_a, int *access_history, int *stride_history) {
    /* Implementation based on the HoPP paper.
     *
     * :param vpn_a: Virtual Page Number of the current access must be the one
     *              at the end of the window?? super confusing.
     * :param stride_a: Stride of the current access i.e. vpn_a - last_vpn_a.
     * :param pid_a: The process ID of the current process (ignored).
     * :param *vpn_history: The array of prior vpn history
     * :param *stride_history: The array of prior strides
     * 
     * @returns :struct prefetcher output:
     */
    struct prefetcher output;

    int max_stride = 2;
    int ripple_num = 0;
    int accumulate_stride = 0;

    if (abs(stride_a) <= max_stride) {
        ripple_num++;
        accumulate_stride = 0;
    }

    for (int i = LENGTH - 2 ; i >= 0; i-- ) {
        accumulate_stride += stride_history[i];
        if (abs(accumulate_stride) <= max_stride) {
            ripple_num++;
            accumulate_stride = 0;
        }
    }
    output.pattern_target = 0;

    // make sure to return the right address. Okay, we need an invalid address
    // marker!
    if (ripple_num >= LENGTH/2) {
        output.valid = true;
        output.stride_target = 1;
    }
    // a stride was not found!
    else {
        // return zeros pretty much!
        output.valid = false;
        output.stride_target = 0;
    }
    return output;
}
                
int main() {
    // I need a duration to run!
   
    // verbose
    bool verbose = true;

    // when the simulation starts, addr holds the current address. assume that
    // his is virtual.
    int addr;

    // I am keeping a count of each window. A window is defined as the duration
    // until the history length is filled up.
    int window = 0;

    // generating traffic until some length
    for (int i = 0 ; i < 100 ; i++) {
        // generate is a yield function.
        addr = generate_traffic(i, "ladder", verbose);
        
        // fill up the access history until the window is completed.
        access_history[i % LENGTH] = addr;

        // if the index is > 0, record the difference between this address and
        // the previous address. This fills up the access stride history table
        // until a window is complete.
        if ((i % LENGTH ) > 0) {
            stride_history[(i % LENGTH) - 1] =
                                       addr - access_history[(i % LENGTH) - 1];
        }
        // process this window. bring in the prefetcher!
        if ((i % LENGTH == 0) && (i != 0)) {

            // TODO add verbose options to not print everything everytime!
            if (verbose == true)
                printf("\t == stats for window %d\n", window++);

            if (verbose == true) {
                printf(" stride_history: ");
                for (int i = 0 ; i < LENGTH - 1; i++) {
                    printf("%d\t", stride_history[i]);
                }
                printf("\n access_history: ");
                for (int i = 0 ; i < LENGTH ; i++) {
                    printf("%d\t", access_history[i]);
                }
            printf("\n");
            }

            // start of a new window!
            // the prefetcher kicks. first use the SSP

            // i think we need to separate out hot pages here! from the window?
            // struct hot_pages = get_hot_pages(int window);
            struct prefetcher results = _dominator();

            if (results.stride_target != -1) {
                // There is a dominating stride!
                // TODO What do I do with it?
                printf("dom stride is %d\n", results.stride_target);
            }
            else {
                printf("no dom stride found!\n");
                // there was no dominating pattern found! try the ladder
                // prefetcher now!
                // need to calculate the current stride. 
                // TODO: Remove this line when we all understand this!
                int stride_a = addr - access_history[LENGTH - 1];

                results = _ladder(
                        addr, stride_a, 0, access_history, stride_history);

                // now make sure to prefetch the addresses
                // as per the paper, this is given by:
                // addr = vpn_a + stride_target + i * pattern_stride

                printf("ladder: %d %d next_addr: %d\n", results.stride_target,
                        results.pattern_target, addr + results.stride_target +
                        results.pattern_target);
                
            }            
        }
    }

    return 0;
}
