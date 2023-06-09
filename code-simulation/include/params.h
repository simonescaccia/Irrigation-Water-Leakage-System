#ifndef PARAM_H
#define PARAM_H 

/** Duty cycle behavior. 
 * 1 ON
 * 0 OFF
*/
#define DUTY_CYCLE 1

/**
 * Simulate one day in seconds
*/
extern const uint32_t SIMULATED_DAY;

/**
 * Estimated time in seconds for sampling and data processing (values difference)
*/
extern const uint32_t SAMPLING_PLUS_DATA_PROCESSING;

/**
 * Choose how often (number of times in a SIMULATED_DAY) we send a message 
 * with the sampled value to the childrens
*/
extern const uint32_t NUMBER_OF_SENDING_PER_DAY;
extern const uint32_t LEAKAGE_TEST_PERIOD;

/**
 * Duty cycle continuous listening
*/
extern const uint32_t LISTENING_TIMEOUT;

typedef struct {
    /* Defining node type: 0 for TTN, 1 for CHIEF, 2 for FORK, 3 for BRANCH */
    int node_type;

    /* Node father and children */
    char* node_source_p2p;
    char* node_self;
    char* node_alt_self;
    char* node_father;
    char** node_children;

    /* Number of children*/
    int children_count;
    /* self children position */
    int self_children_position;
} node_t;

typedef struct {
    float* water_flow;        /* Array that contains all the sensor sampling value */
    float water_flow_sum;     /* The sum of all the sensor sampling */
} sample_t;

#endif
