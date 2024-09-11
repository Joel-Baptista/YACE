#include "game.h"


struct buffer_items
{
    torch::Tensor states;
    torch::Tensor action_probs;
    torch::Tensor values;
};

class ReplayBuffer
{
    public:
        ReplayBuffer(int buffer_size);
        ~ReplayBuffer();
        void add(torch::Tensor encoded_state, torch::Tensor action_probs, torch::Tensor value);
        void reset();
        void adding_new_game() { current_game_id++; }
        int get_current_game_id() { return current_game_id; }
        buffer_items sample(int batch_size);
        int size();
    
    private:

        int pos;
        bool full;
        int buffer_size;
        torch::Tensor states;
        torch::Tensor action_probs;
        torch::Tensor values;
        std::unique_ptr<int> indices;
        int* pGameIds;
        int current_game_id;

};