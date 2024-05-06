
#include <numeric>
#include "header/BounceFilter.h"

BounceFilter::BounceFilter(float memory_kb, Sketch* sketch1): sketch(sketch1){
    d = 3;
    bits[0] = 4;
    bits[1] = 8;

    max_positive[0] = (1 << (bits[0] - 1)) - 1;
    max_positive[1] = (1 << (bits[1] - 1)) - 1;
    min_negative[0] = -(1 << (bits[0] - 1));
    min_negative[1] = -(1 << (bits[1] - 1));

    uint32_t memory_bits = static_cast<uint32_t>(std::round(memory_kb * 1024 * 8));

    // Memory allocation: BF two layers of memory each occupy 50%
    uint32_t memory_bits_layer_0 = static_cast<uint32_t>(std::round(memory_bits * 0.5));
    uint32_t memory_bits_layer_1 = memory_bits - memory_bits_layer_0;

    num_counters[0] = static_cast<uint32_t>(std::round(memory_bits_layer_0 / bits[0]));
    num_counters[1] = static_cast<uint32_t>(std::round(memory_bits_layer_1 / bits[1]));

    counters = new int *[2];
    // Allocate memory for int arrays
    counters[0] = new int[num_counters[0]]{0}; // layer 1
    counters[1] = new int[num_counters[1]]{0}; // layer 2

    // Initialize counters to 0
//    for (int i = 0; i < 2; ++i) {
//        for(int j = 0; j < num_counters[i]; j++)
//            counters[i][j] = 0;
//    }

}


int BounceFilter::hash_s(const char* flow_label, uint32_t& counter_index) {
    uint32_t hash_value = 0;
    MurmurHash3_x86_32(flow_label, KEY_LEN, counter_index, &hash_value);
    return hash_value % 2 == 0;
}


void BounceFilter::update(const int packet_id, const char* flow_label, uint32_t weight){

    uint32_t index_hash; // used to calculate the counter index
    uint32_t rand_value = packet_id % d;
    MurmurHash3_x86_32(flow_label, KEY_LEN, rand_value, &index_hash);

    for (int i = 0; i < 2; ++i) {
        uint32_t j = index_hash % num_counters[i];
        int op_ = hash_s(flow_label, j);
        int current_value = counters[i][j];
        int next_value;
        switch (op_) {
            case 0:
                next_value = current_value - 1;
                if (next_value < min_negative[i])
                    continue;
                break;
            case 1:
                next_value = current_value + 1;
                if (next_value > max_positive[i])
                    continue;
                break;
            default:
                // Handle unexpected values of op_ here
                return;
        }

        counters[i][j] = next_value;
        return;

    }
    sketch->update(packet_id, flow_label);

}


int BounceFilter::report(const char* flow_label){
    int estimation = 0;
    uint32_t hash_value = 0;
    bool large_flow_flag = true;

    for (int i = 0; i < 2; ++i) {
        int es_per_layer = 0;
        for (int c = 0; c < d; c++) {
            MurmurHash3_x86_32(flow_label, KEY_LEN, c, &hash_value);
            uint32_t j = hash_value % num_counters[i];
            int op_ = hash_s(flow_label, j);
            int es_ = counters[i][j] * op_;
            es_per_layer += es_;
        }
        estimation += es_per_layer;
        // if the estimated size less than counter capacity of this layer
        if (es_per_layer < d * max_positive[i] ){
            large_flow_flag = false;
            break;
        }
    }
    if(large_flow_flag){
        estimation += sketch->report(flow_label);
    }

    return estimation;
}
