#include "Bounded_Search.hpp"
#include "Profitable_Moves_Enumerators.hpp"
#include "process_each_node.hpp"
#include "src/matOptimize/mutation_annotated_tree.hpp"
#include "src/matOptimize/tree_rearrangement_internal.hpp"
#include <algorithm>
#include <climits>
#include <cstdint>
#include "split_node_helpers.hpp"
#include <cstdio>
#include <utility>
typedef Bounded_Mut_Change_Collection::const_iterator Bounded_Mut_Iter;
static void
add_remaining_dst_to_LCA_nodes(MAT::Node *cur, const MAT::Node *LCA,
                               std::vector<MAT::Node *> &dst_stack) {
    while (cur != LCA) {
        // assert(dst_stack.empty()||dst_stack.back()!=cur);
        dst_stack.push_back(cur);
        cur = cur->parent;
    }
}

void output_result(MAT::Node *src, MAT::Node *dst, MAT::Node *LCA,
                   int parsimony_score_change, output_t &output,
                   const std::vector<MAT::Node *> &node_stack_from_src,
                   std::vector<MAT::Node *> &node_stack_from_dst,
                   std::vector<MAT::Node *> &node_stack_above_LCA,
                   int radius_left);
static void output_not_LCA(Mutation_Count_Change_Collection &parent_added,
                    MAT::Node *dst_node, int parsimony_score_change,
                    int lower_bound, const src_side_info &src_side,
                    int radius
#ifdef CHECK_BOUND
    ,bool do_continue,bool is_last
#endif
                    ) {
    if (use_bound&&lower_bound > src_side.out.score_change) {
#ifndef CHECK_BOUND
        return;
#else
        do_continue=false;
        assert(!is_last);
#endif
    }
    std::vector<MAT::Node *> node_stack_from_dst;
    Mutation_Count_Change_Collection parent_of_parent_added;
    parent_of_parent_added.reserve(parent_added.size());
    node_stack_from_dst.push_back(dst_node);
    auto this_node = dst_node->parent;
    if (dst_node->dfs_index==33740) {
        //fputc('a', stderr);
    }
    while (this_node != src_side.LCA) {
        parent_of_parent_added.clear();
        get_intermediate_nodes_mutations(this_node, node_stack_from_dst.back(),
                                         parent_added, parent_of_parent_added,
                                         parsimony_score_change);
        node_stack_from_dst.push_back(this_node);
        parent_added.swap(parent_of_parent_added);
        if (parent_added.empty()) {
            add_remaining_dst_to_LCA_nodes(this_node->parent, src_side.LCA,
                                           node_stack_from_dst);
            break;
        }
        this_node = this_node->parent;
    }
    std::vector<MAT::Node *> node_stack_above_LCA;
    parent_of_parent_added.reserve(
        parent_added.size() + src_side.allele_count_change_from_src.size());
    // Adjust LCA node and above
    parent_of_parent_added.clear();
    bool is_src_terminal = src_side.src->parent == src_side.LCA;
    if ((!(src_side.allele_count_change_from_src.empty() &&
           parent_added.empty())) ||
        is_src_terminal) {
        get_LCA_mutation(src_side.LCA,
                         is_src_terminal ? src_side.src
                                         : src_side.node_stack_from_src.back(),
                         is_src_terminal, src_side.allele_count_change_from_src,
                         parent_added, parent_of_parent_added,
                         parsimony_score_change);
    }
    node_stack_above_LCA.push_back(src_side.LCA);
    parent_added.swap(parent_of_parent_added);
    parent_of_parent_added.clear();
    check_parsimony_score_change_above_LCA(
        src_side.LCA, parsimony_score_change, parent_added,
        src_side.node_stack_from_src, node_stack_above_LCA,
        parent_of_parent_added, src_side.LCA->parent);
    #ifdef CHECK_PAR_MAIN
    output_t temp;
    auto refout=individual_move(src_side.src, dst_node, src_side.LCA, temp);
    if (refout>=0) {
    if (parsimony_score_change<0) {
        fprintf(stderr, "%s\n",src_side.src->identifier.c_str());
    }
    }else {
    if (parsimony_score_change!=refout) {
        fprintf(stderr, "%s\n",src_side.src->identifier.c_str());
    }
    }
    #endif
    #ifdef CHECK_BOUND
    if (!((!use_bound)||(parsimony_score_change >= lower_bound)||(parsimony_score_change>=0))) {
        fprintf(stderr, "%s\n",src_side.src->identifier.c_str());
    }
    if (parsimony_score_change<=src_side.out.score_change) {
        //assert(do_continue);
    }else if (!do_continue) {
        src_side.savings.saved++;
    }
    src_side.savings.total++;
#endif
    output_result(src_side.src, dst_node, src_side.LCA, parsimony_score_change,
                  src_side.out, src_side.node_stack_from_src,
                  node_stack_from_dst, node_stack_above_LCA, radius);
}
struct go_descendant{};
struct no_descendant{};
static void add_mut(Mutation_Count_Change_W_Lower_Bound_Downward &mut, int radius_left,
                    MAT::Node *node, int &descendant_lower_bound,
                    Bounded_Mut_Change_Collection &mut_out, go_descendant) {
    mut_out.emplace_back(mut, node, radius_left);
    if (!mut_out.back().valid_on_subtree()) {
        descendant_lower_bound++;
    }
}
static void add_mut(Mutation_Count_Change_W_Lower_Bound_Downward &mut, int radius_left,
                    MAT::Node *node, int &descendant_lower_bound,
                    Bounded_Mut_Change_Collection &mut_out, no_descendant) {}

static void add_mut(Mutation_Count_Change_W_Lower_Bound_Downward &mut, int radius_left,
                    MAT::Node *node, int &descendant_lower_bound,
                    Bounded_Mut_Change_Collection &mut_out,
                    const MAT::Mutation &coincided_mut) {
    mut_out.emplace_back(mut, node, radius_left, coincided_mut);
    if (!mut_out.back().valid_on_subtree()) {
        descendant_lower_bound++;
    }
}
static void unmatched(Bounded_Mut_Change_Collection &mut_out,
                      const MAT::Mutation &mut, MAT::Node *node,
                      int radius_left, int &descendant_lower_bound,
                      go_descendant) {
    if (mut.is_valid()) {
        mut_out.emplace_back(
            mut, node, radius_left);
        if (use_bound && !mut_out.back().valid_on_subtree()) {
            descendant_lower_bound++;
        }
        // assert((!mut_out.back().offsetable)||mut_out.back().have_content);
    }
}
static void unmatched(Bounded_Mut_Change_Collection & mut_out, const MAT::Mutation& mut,MAT::Node *node, int radius_left,int& descendant_lower_bound,no_descendant){
}
template<typename go_descendant , typename S>
int downward_integrated(MAT::Node *node, int radius_left,
                        Bounded_Mut_Change_Collection &from_parent,
                        Bounded_Mut_Change_Collection &mut_out,
                        const src_side_info &src_side,go_descendant go_des_tag,S ignore_iter
#ifdef CHECK_BOUND
                        ,
                        int prev_lower_bound,bool do_continue,bool first_level
#endif

) {
    auto iter = from_parent.begin();
    mut_out.reserve(from_parent.size()+node->mutations.size());
    Mutation_Count_Change_Collection split_allele_cnt_change;
    split_allele_cnt_change.reserve(from_parent.size()+ node->mutations.size());
    int par_score_from_split = src_side.par_score_change_from_src_remove;
    int par_score_from_split_lower_bound=src_side.src_par_score_lower_bound;
    int descendant_lower_bound = src_side.src_par_score_lower_bound;
    if (node->dfs_index==33740) {
        //fputc('a', stderr);
    }
    for (const auto &mut : node->mutations) {
        if (mut.get_position()==10523&&node->dfs_index==23189) {
            //fputc('a', stderr);
        }
        if (ignore_iter(mut.get_position())) {
            continue;
        }
        while (iter->get_position() < mut.get_position()) {
            add_node_split(
                *iter, split_allele_cnt_change, par_score_from_split);
            if (!(iter->get_incremented()&iter->get_senesitive_increment())) {
                par_score_from_split_lower_bound++;
            }else {
                //assert((iter->have_content)||iter->par_sensitive_increment&iter->get_par_state());
            } 
            add_mut(*iter, radius_left, node, descendant_lower_bound, mut_out,go_des_tag);
            iter++;
        }
        if (iter->get_position() == mut.get_position()) {
            add_node_split(
                mut, mut.get_all_major_allele(), iter->get_incremented(),
                split_allele_cnt_change, par_score_from_split);
            if (!(iter->get_incremented()&mut.get_sensitive_increment())) {
                par_score_from_split_lower_bound++;
            }else {
                //assert((iter->have_content)||iter->par_sensitive_increment&iter->get_par_state());
            }
            if (mut.get_mut_one_hot()!=iter->get_incremented()) {
                add_mut(*iter, radius_left, node, descendant_lower_bound, mut_out,mut);
            }
            iter++;
        } else {
            add_node_split(mut, split_allele_cnt_change, par_score_from_split);
            unmatched(mut_out, mut, node, radius_left, descendant_lower_bound, go_des_tag);
        }
    }
    while (iter ->get_position()!=INT_MAX) {
        add_node_split(
            *iter, split_allele_cnt_change, par_score_from_split);
        if (!(iter->get_incremented()&iter->get_senesitive_increment())) {
            par_score_from_split_lower_bound++;
        }
        add_mut(*iter, radius_left, node, descendant_lower_bound, mut_out,go_des_tag);
        iter++;
    }
    mut_out.emplace_back();
#ifdef CHECK_BOUND
    if(!((!use_bound)||par_score_from_split_lower_bound >= prev_lower_bound)){
        fprintf(stderr, "%s\n",src_side.src->identifier.c_str());
    }
#endif

    output_not_LCA(split_allele_cnt_change, node, par_score_from_split,
                   par_score_from_split_lower_bound, src_side, radius_left
#ifdef CHECK_BOUND
    ,do_continue,((!first_level)&&(node->is_leaf()||(!radius_left)||descendant_lower_bound>src_side.out.score_change))
#endif
                   );
    return descendant_lower_bound;
}
template<typename T>
static void search_subtree_bounded_internal(MAT::Node *node, const src_side_info &src_side,
                    int radius_left,
                    Bounded_Mut_Change_Collection &par_muts,
                    int lower_bound,T tag
#ifdef CHECK_BOUND
                                      ,bool do_continue,bool first_level
#endif
                    ) {
    Bounded_Mut_Change_Collection muts;
    if (radius_left&&node->children.size()) {
            lower_bound = downward_integrated(node, radius_left, par_muts, muts,
                                      src_side,go_descendant(), T(src_side.src->ignore)
#ifdef CHECK_BOUND
                                      ,lower_bound,do_continue,first_level
#endif
                                      );
    }else {
                    lower_bound = downward_integrated(node, radius_left, par_muts, muts,
                                      src_side,no_descendant(), T(src_side.src->ignore)
#ifdef CHECK_BOUND
                                      ,lower_bound,do_continue,first_level
#endif
                                      );
    }

    if (!radius_left) {
        return;
    }
    if (use_bound&&lower_bound > src_side.out.score_change) {
#ifndef CHECK_BOUND
        return;
#else
        do_continue=false;
#endif
    }
    for (auto child : node->children) {
        search_subtree_bounded(child, src_side, radius_left-1, muts, lower_bound,tag
#ifdef CHECK_BOUND
,do_continue,first_level
#endif
        );
    }
}
void search_subtree_bounded(MAT::Node *node, const src_side_info &src_side,
                    int radius_left,
                    Bounded_Mut_Change_Collection &par_muts,
                    int lower_bound,ignore_ranger_nop tag
#ifdef CHECK_BOUND
                                      ,bool do_continue,bool first_level
#endif
                    ){
                        search_subtree_bounded_internal(node, src_side,radius_left,par_muts,lower_bound,tag
#ifdef CHECK_BOUND
                                      ,do_continue, first_level
#endif
                    );
                    }
void search_subtree_bounded(MAT::Node *node, const src_side_info &src_side,
                    int radius_left,
                    Bounded_Mut_Change_Collection &par_muts,
                    int lower_bound,ignore_ranger tag
#ifdef CHECK_BOUND
                                      ,bool do_continue,bool first_level
#endif
                    ){
                                                search_subtree_bounded_internal(node, src_side,radius_left,par_muts,lower_bound,tag
#ifdef CHECK_BOUND
                                      ,do_continue, first_level
#endif
                    );
                    }