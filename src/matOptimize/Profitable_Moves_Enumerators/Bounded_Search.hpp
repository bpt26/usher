#include "Profitable_Moves_Enumerators.hpp"
#include "src/matOptimize/mutation_annotated_tree.hpp"
#include "src/matOptimize/tree_rearrangement_internal.hpp"
#include <algorithm>
#include <array>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <utility>
#include <vector>
struct src_side_info {
    output_t &out;
#ifdef CHECK_BOUND
    counters &savings;
#endif
    int par_score_change_from_src_remove;
    int src_par_score_lower_bound;
    MAT::Node *LCA;
    MAT::Node *src;
    Mutation_Count_Change_Collection allele_count_change_from_src;
    std::vector<MAT::Node *> node_stack_from_src;
};
struct ignore_ranger_nop{
    ignore_ranger_nop(const Mutation_Annotated_Tree::ignored_t& in){}
    ignore_ranger_nop(){}
    bool operator()(int){return false;}
};
struct ignore_ranger{
    Mutation_Annotated_Tree::ignored_t::const_iterator iter;
    ignore_ranger(const Mutation_Annotated_Tree::ignored_t& in){
        iter=in.begin();
    }
    bool operator()(int pos){
        while (iter->second<pos) {
            iter++;
        }
        return iter->first<=pos&&iter->second>=pos;
    }
};
class Mutation_Count_Change_W_Lower_Bound_Downward : public Mutation_Count_Change {
    static const uint8_t LEVEL_END = UINT8_MAX;
    uint8_t par_sensitive_increment;
    uint8_t next_level;
    uint16_t idx;
    void to_descendant_adjust_range(Mutation_Count_Change_W_Lower_Bound_Downward &in,
                                    const MAT::Node *node, int level_left) {
        if (node->dfs_index==19407&&get_position()==227703&&get_incremented()==4) {
            fputc('a', stderr);
        }
        if (in.next_level > node->level) {
            return;
        }
        if (in.idx==EMPTY_POS) {
            next_level = LEVEL_END;
            return;
        }
        const auto &addable_idxes_this_pos = addable_idxes[get_position()];
        auto end_idx = node->dfs_end_index;
        assert(in.idx==EMPTY_POS||in.idx<addable_idxes_this_pos.nodes.size());
        for (;
             addable_idxes_this_pos.nodes[in.idx].dfs_end_idx < node->dfs_index;
             in.idx++) {
        }
        if (addable_idxes_this_pos.nodes[in.idx].dfs_start_idx > end_idx) {
            in.next_level = LEVEL_END;
            idx = EMPTY_POS;
        }
        assert(in.idx==addable_idxes_this_pos.find_idx(node));
        if (idx==EMPTY_POS) {
            return;
        }
        if (addable_idxes_this_pos.nodes[in.idx].level==node->level) {
            if (addable_idxes_this_pos.nodes[in.idx].children_start_idx==EMPTY_POS) {
                in.next_level = LEVEL_END;
                idx = EMPTY_POS;
                return;
            }else {
                idx=addable_idxes_this_pos.nodes[in.idx].children_start_idx;
                in.next_level=0;
                return;
            }
        }
        assert(addable_idxes_this_pos.nodes[in.idx].level>node->level);
        next_level =LEVEL_END ;
        idx = in.idx;
        // accumulate closest level
        for (; addable_idxes_this_pos.nodes[in.idx].dfs_start_idx < end_idx;
             in.idx++) {
            const auto &addable = addable_idxes_this_pos.nodes[in.idx];
            assert(addable.dfs_end_idx < end_idx);
            if (test_level(level_left, addable)) {
                if (addable.level < next_level) {
                    next_level = addable.level;
                }
            }
        }
    }
    // going to descendant
    bool test_level(int radius_left, const range_tree_node &node) {
        for (int idx = 0; idx < 4; idx++) {
            if (get_incremented() & (1 << idx)) {
                if (node.min_level[idx] < radius_left) {
                    return true;
                }
            }
        }
        return false;
    }

  public:
    uint8_t get_senesitive_increment()const{
        return par_sensitive_increment;
    }
    void set_sensitive_increment(uint8_t in){
        par_sensitive_increment=in;
    }
    Mutation_Count_Change_W_Lower_Bound_Downward(
        Mutation_Count_Change& base,
        uint8_t par_sensitive_increment,uint8_t next_level,uint16_t idx)
        :Mutation_Count_Change(base),par_sensitive_increment(par_sensitive_increment)
        ,next_level(next_level),idx(idx){}
    // Going down no coincide
    Mutation_Count_Change_W_Lower_Bound_Downward(){}
    Mutation_Count_Change_W_Lower_Bound_Downward(Mutation_Count_Change_W_Lower_Bound_Downward &in,
                                        const MAT::Node *node, int level_left)
        : Mutation_Count_Change_W_Lower_Bound_Downward(in) {
        to_descendant_adjust_range(in, node, level_left);
        set_sensitive_increment(in.get_par_state());
    }
    // Going Down coincide
    Mutation_Count_Change_W_Lower_Bound_Downward(Mutation_Count_Change_W_Lower_Bound_Downward &in,
                                        const MAT::Node *node, int level_left,
                                        const MAT::Mutation &coincided_mut)
        : Mutation_Count_Change_W_Lower_Bound_Downward(in) {
        to_descendant_adjust_range(in, node, level_left);
        set_sensitive_increment( coincided_mut.get_sensitive_increment() &
                                  (coincided_mut.get_all_major_allele() |
                                   coincided_mut.get_boundary1_one_hot()));
        //assert(get_senesitive_increment() != coincided_mut.get_mut_one_hot());
    }

    // Going down new
    Mutation_Count_Change_W_Lower_Bound_Downward(const MAT::Mutation &in,
                                        const MAT::Node *node, int level_left)
        : Mutation_Count_Change(in, 0, in.get_par_one_hot()) {
        assert(in.is_valid());
        set_par_nuc(in.get_mut_one_hot());
        set_sensitive_increment(in.get_sensitive_increment() &
                                  (in.get_all_major_allele() |
                                   in.get_boundary1_one_hot()));
        const auto& this_possition_addable_idx=addable_idxes[get_position()];
        auto start_idx=this_possition_addable_idx.find_idx(node);
        if (start_idx==UINT16_MAX) {
            next_level=UINT8_MAX;
            return;
        }else if (this_possition_addable_idx.nodes[start_idx].level==node->level) {
            start_idx=this_possition_addable_idx.nodes[start_idx].children_start_idx;
            if (start_idx==UINT16_MAX) {
                next_level=UINT8_MAX;
                return;
            }
        }
        assert(this_possition_addable_idx.nodes[start_idx].level>node->level);
        idx=start_idx;
        if (next_level != LEVEL_END) {
            for (; addable_idxes[get_position()].nodes[idx].dfs_start_idx <
                   node->dfs_end_index;
                 idx++) {
                const auto &addable = addable_idxes[get_position()].nodes[idx];
                if (test_level(level_left, addable)) {
                    if (addable.level < next_level) {
                        next_level = addable.level;
                    }
                }
            }
        }
    }
    bool valid_on_subtree()const{
        return next_level!=LEVEL_END||(get_incremented()&get_par_state());
    }
};
class Mutation_Count_Change_W_Lower_Bound_to_ancestor : public Mutation_Count_Change {
    static const uint8_t LEVEL_END = -1;
    uint8_t next_level;
    uint16_t idx;
    void init(const MAT::Node *node) {
        assert(use_bound);
        const auto& this_aux=addable_idxes[get_position()];
        uint16_t next_idx=EMPTY_POS;
        if (get_position()==2652&&node->dfs_index==4505) {
            fputc('a', stderr);
        }
        idx =this_aux.find_idx(node,next_idx);
        if(next_idx==EMPTY_POS){
            next_level=LEVEL_END;
            assert(get_incremented()&get_par_state());
        }else {
            next_level = this_aux.nodes[next_idx].level;            
        }
    }
    void output_absent_sibling(std::vector<Mutation_Count_Change_W_Lower_Bound_Downward>& sibling_out){
        sibling_out.emplace_back(*this,get_par_state(),UINT8_MAX,UINT16_MAX);
    }
  public:
    void to_ancestor_adjust_range(const MAT::Node *node,std::vector<Mutation_Count_Change_W_Lower_Bound_Downward>& sibling_out) {
        assert(get_incremented()!=get_par_state());
        if (node->level>next_level) {
            output_absent_sibling(sibling_out);
            return;
        }
        const auto &addable_idxes_this_pos = addable_idxes[get_position()];
        if (idx==EMPTY_POS) {
            idx =addable_idxes_this_pos.find_idx(node);
            assert(idx!=EMPTY_POS||(get_incremented()&&get_par_state()));
        }else {
        idx = addable_idxes_this_pos.nodes[idx].parent_idx;
        while (idx != EMPTY_POS &&
               addable_idxes_this_pos.nodes[idx].level >=
                   node->level) {
            idx = addable_idxes_this_pos.nodes[idx].parent_idx;
        }
        if (idx==EMPTY_POS||addable_idxes_this_pos.nodes[idx].children_start_idx==EMPTY_POS) {
            output_absent_sibling(sibling_out);
            return;
        }
        idx=addable_idxes_this_pos.nodes[idx].children_start_idx;
        next_level=addable_idxes_this_pos.nodes[idx].level;
        for(;addable_idxes_this_pos.nodes[idx].dfs_start_idx<node->dfs_index;idx++){}
        if (addable_idxes_this_pos.nodes[idx].dfs_start_idx>node->dfs_end_index) {
            idx=EMPTY_POS;
        }
        assert(idx==addable_idxes_this_pos.find_idx(node));
        }
        uint16_t start_idx=EMPTY_POS;
        if (idx != EMPTY_POS) {
            start_idx = addable_idxes_this_pos.nodes[idx].children_start_idx;
        }
        if (start_idx == EMPTY_POS){
            output_absent_sibling(sibling_out);
            return;
        }else {
            sibling_out.emplace_back(*this,get_par_state(),0,start_idx);
        }
    }
    Mutation_Count_Change_W_Lower_Bound_to_ancestor(){}
    // Going Up not Coincide
    Mutation_Count_Change_W_Lower_Bound_to_ancestor(const Mutation_Count_Change_W_Lower_Bound_to_ancestor &in,
                                        const MAT::Node *node,std::vector<Mutation_Count_Change_W_Lower_Bound_Downward>& sibling_out)
        : Mutation_Count_Change_W_Lower_Bound_to_ancestor(in) {
        assert(get_incremented()!=get_par_state());
        to_ancestor_adjust_range(node,sibling_out);
    }
    // Going up coincide
    Mutation_Count_Change_W_Lower_Bound_to_ancestor(const Mutation_Count_Change_W_Lower_Bound_to_ancestor &in,
                                        const MAT::Node *node,
                                        const MAT::Mutation &coincided_mut,std::vector<Mutation_Count_Change_W_Lower_Bound_Downward>& sibling_out
                            )
        : Mutation_Count_Change_W_Lower_Bound_to_ancestor(in) {
        assert(get_incremented()!=get_par_state());
        to_ancestor_adjust_range(node,sibling_out);
        set_par_nuc(coincided_mut.get_par_one_hot());
        auto par_sensitive_increment = coincided_mut.get_sensitive_increment() &
                                  (coincided_mut.get_all_major_allele() |
                                   coincided_mut.get_boundary1_one_hot());
        sibling_out.back().set_sensitive_increment(par_sensitive_increment);
        //assert(par_sensitive_increment != coincided_mut.get_mut_one_hot());
    }
    // Going up new
    Mutation_Count_Change_W_Lower_Bound_to_ancestor(const MAT::Mutation &in,
                                        const MAT::Node *node)
        : Mutation_Count_Change(in, 0, in.get_mut_one_hot()) {
        assert(get_incremented()!=get_par_state());
        set_par_nuc(in.get_par_one_hot());
        init(node);
    }
    struct source_node{};
    Mutation_Count_Change_W_Lower_Bound_to_ancestor(const MAT::Mutation &in,
                                        const MAT::Node *node,source_node)
        : Mutation_Count_Change(in, 0, in.get_all_major_allele()) {
        assert(get_incremented()!=get_par_state());
        set_par_nuc(in.get_par_one_hot());
        init(node);
    }
};
typedef std::vector<Mutation_Count_Change_W_Lower_Bound_Downward>
    Bounded_Mut_Change_Collection;
void search_subtree_bounded(MAT::Node *node, const src_side_info &src_side,
                    int radius_left,
                    Bounded_Mut_Change_Collection &par_muts,
                    int lower_bound,ignore_ranger_nop
#ifdef CHECK_BOUND
                                      ,bool do_continue,bool first_level
#endif
                    );
void search_subtree_bounded(MAT::Node *node, const src_side_info &src_side,
                    int radius_left,
                    Bounded_Mut_Change_Collection &par_muts,
                    int lower_bound,ignore_ranger
#ifdef CHECK_BOUND
                                      ,bool do_continue,bool first_level
#endif
                    );