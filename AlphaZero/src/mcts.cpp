#include "mcts.h"
#include "game.h" 
#include <cmath>


MCTS::MCTS(std::shared_ptr<ResNetChess> model, int num_searches, float dichirlet_alpha, float dichirlet_epsilon, float C)
{
    this->num_searches = num_searches;
    this->dichirlet_alpha = dichirlet_alpha;
    this->dichirlet_epsilon = dichirlet_epsilon;
    this->C = C;
    this->m_model = model;
}

MCTS::~MCTS()
{
}


void MCTS::search(std::vector<SPG*>* spGames)
{
    auto st = get_time_ms();
    std::vector<int64_t> shape = {(long)spGames->size(), 19, 8, 8};
    torch::Tensor encoded_state = torch::zeros(shape, torch::kFloat32); // Initialize the tensor with zeros
    for (int i = 0; i < spGames->size(); i++)
    {
        std::vector<int64_t> shape = {(long)spGames->size(), 19, 8, 8};
        torch::Tensor state = torch::zeros({1, 19, 8, 8}, torch::kFloat32); // Initialize the tensor with zeros
        get_encoded_state(state, spGames->at(i)->current_state);
        encoded_state[i] = state.squeeze(0); 
    }

    chess_output output_roots;
    {
        torch::NoGradGuard no_grad;
        encoded_state = encoded_state.to(*m_model->m_Device);
        output_roots = m_model->forward(encoded_state);
        int batch_size = output_roots.policy.size(0);
        
        output_roots.policy = torch::softmax(output_roots.policy.view({output_roots.policy.size(0), -1}), 1).view({-1, 8, 8, 73});

        torch::Tensor noise = torch::zeros({batch_size, 8, 8, 73});

        dirichlet_noise(noise, dichirlet_alpha, batch_size);

        noise = noise.to(*m_model->m_Device);
    
        // // Apply Dirichlet noise to the policy
        output_roots.policy = (1 - dichirlet_epsilon) * output_roots.policy + dichirlet_epsilon * noise;   
    }
    // std::cout << "Time to get policy: " << ((get_time_ms() - st) / 1000.0f) << " seconds" << std::endl;
    st = get_time_ms();
    for (int i = 0; i < spGames->size(); i++)
    {
        std::vector<int64_t> shape = {8, 8, 73}; 
        torch::Tensor spg_policy = output_roots.policy[i].cpu();
        torch::Tensor valid_moves = torch::zeros(shape, torch::kFloat32);

        spGames->at(i)->game->set_state(spGames->at(i)->current_state);

        moves move_list;
        spGames->at(i)->game->m_Board->get_alpha_moves(&move_list);

        get_valid_moves_encoded(valid_moves, spGames->at(i)->current_state, move_list);
        spg_policy *= valid_moves;
        spg_policy /= spg_policy.sum();

        spGames->at(i)->pRoot = new Node(spGames->at(i)->game, nullptr, "", C, 0.0, 1);

        
        spGames->at(i)->pRoot->expand(spg_policy, valid_moves);
        

    }
    // std::cout << "Time to expand root: " << ((get_time_ms() - st) / 1000.0f) << " seconds" << std::endl;
    for (int i = 0; i < num_searches; i++)
    {
        st = get_time_ms();
        for (int j = 0; j < spGames->size(); j++)
        {
            spGames->at(j)->pCurrentNode = nullptr;
            Node* pNode = spGames->at(j)->pRoot;
            
            while (pNode->is_fully_expanded())
            {
                pNode = pNode->select();
            }

            
            final_state fState = spGames->at(j)->game->get_value_and_terminated(pNode->node_state, spGames->at(j)->repeated_states);
            

            if (fState.terminated)
            {
                pNode->backpropagate(-fState.value);
            }
            else
            {
                spGames->at(j)->pCurrentNode = pNode;
            }    

        }
        std::vector<int> expandable_games;

        for (int k = 0; k < spGames->size(); k++)
        {
            if (spGames->at(k)->pCurrentNode != nullptr)
            {
                expandable_games.push_back(k);
            }
        }
// 
        // std::cout << "Time to select node: " << ((get_time_ms() - st) / 1000.0f) << " seconds" << std::endl;
        st = get_time_ms();
        
        chess_output output_exapandables;
        if (expandable_games.size() > 0)
        {
            std::vector<int64_t> exp_shape = {(long)expandable_games.size(), 19, 8, 8};
            torch::Tensor encoded_states = torch::zeros(exp_shape, torch::kFloat32); // Initialize the tensor with zeros

            for (int i = 0; i < expandable_games.size(); i++)
            {
                int game_index = expandable_games[i];
                torch::Tensor state = torch::zeros({1, 19, 8, 8}, torch::kFloat32); // Initialize the tensor with zeros
                get_encoded_state(state, spGames->at(game_index)->current_state);
                encoded_states[i] = state.squeeze(0); 
            }
            {
                torch::NoGradGuard no_grad;
                encoded_states = encoded_states.to(*m_model->m_Device);
                output_exapandables = m_model->forward(encoded_states);

                output_exapandables.policy = torch::softmax(output_exapandables.policy.view({output_exapandables.policy.size(0), -1}), 1).view({-1, 8, 8, 73});
            }

        }
        
        // std::cout << "Time to policy expandables: " << ((get_time_ms() - st) / 1000.0f) << " seconds" << std::endl;
        st = get_time_ms();

        for (int k = 0; k < expandable_games.size(); k++)
        {
            int game_index = expandable_games[k];

            std::vector<int64_t> shape = {8, 8, 73}; 
            torch::Tensor spg_policy = output_exapandables.policy[k].cpu();
            torch::Tensor valid_moves = torch::zeros(shape, torch::kFloat32);

            spGames->at(game_index)->game->set_state(spGames->at(game_index)->current_state);

            moves move_list;
            spGames->at(game_index)->game->m_Board->get_alpha_moves(&move_list);

            get_valid_moves_encoded(valid_moves, spGames->at(game_index)->current_state, move_list);

            spg_policy *= valid_moves;
            spg_policy /= spg_policy.sum();

            spGames->at(game_index)->pCurrentNode->expand(spg_policy, valid_moves);

            st = get_time_ms();
            spGames->at(game_index)->pCurrentNode->backpropagate(output_exapandables.value[k].cpu().item<float>());
        }

        // std::cout << "Time to expand expandables: " << ((get_time_ms() - st) / 1000.0f) << " seconds" << std::endl;
        
    }

}

Node::Node(std::shared_ptr<Game> game, Node* parent, std::string action, float C, float prior, int visit_count)
{

    this->game = game;

    copy_state_from_board(node_state, game->m_Board);

    this->parent = parent;
    this->action = action;
    this->C = C;
    this->prior = prior;
    this->visit_count = visit_count;
    this->value_sum = 0;
}

Node::~Node()
{
}

bool Node::is_fully_expanded()
{
    return pChildren.size() > 0;
}
Node* Node::select()
{
    Node* selected_child = nullptr;
    float best_ubc = -1000000;

    for (int i = 0; i < pChildren.size(); i++)
    {
        Node* child = pChildren[i];

        float ubc = get_ubc(child);

        if (ubc > best_ubc)
        {
            best_ubc = ubc;
            selected_child = child;
        }
    }

    // copy_alpha_board(game->m_Board);
    // game->set_state(node_state);
    // game->m_Board->print_board();
    // restore_alpha_board(game->m_Board);

    return selected_child;
}
float Node::get_ubc(Node* child)
{
    float q_value;

    if (child->visit_count == 0)
    {
        q_value = 0;
    }
    else
    {
        q_value = ((child->value_sum / child->visit_count) + 1.0f) / 2.0f;
    }


    return q_value + child->prior * C * std::sqrt(visit_count) / (1 + child->visit_count);

}
void Node::expand(torch::Tensor action_probs, torch::Tensor valid_moves)
{

    game->set_state(node_state);  
    auto st = get_time_ms();
    auto decoded_actions = game->decode_actions(node_state, action_probs, valid_moves);

    int count = 0;

    for (int i = 0; i < decoded_actions.size(); i++)
    {
            std::string action = decoded_actions[i].action;
            float prob = decoded_actions[i].probability;
            
            copy_alpha_board(game->m_Board);

            int valid_move = game->m_Board->make_move(game->m_Board->parse_move(action.c_str()));

            if (valid_move)
            {   
                count++;
                Node* new_node = new Node(game, this, action, C, prob, 0);
                pChildren.push_back(new_node);
            }

            restore_alpha_board(game->m_Board);

    }   
}
void Node::backpropagate(float value)
{

    visit_count++;
    value_sum += value;

    if (parent != nullptr)
    {   
        parent->backpropagate(-value);
    }
}


SPG::SPG(std::shared_ptr<Game> game)
{

    this->game = game;

    copy_state_from_board(initial_state, game->m_Board);
    copy_state(current_state, initial_state);

}

SPG::~SPG()
{
}
