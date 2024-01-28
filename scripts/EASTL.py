// manual conversion of EASTL.natvis

// see here for string, table, sequences for Nim language
// https://www.reddit.com/r/nim/comments/lhaaa6/debugging_support_formatters_for_lldb_in_vscode/

// https://pspdfkit.com/blog/2018/how-to-extend-lldb-to-provide-a-better-debugging-experience/debugging_support_formatters_for_lldb_in_vscode
// TODO: size is template param

// run this from .lldbinit 
// command script import source kram/scripts/EASTL.py

// can also add commands to lldb that execute when run

import lldb

def __lldb_init_module(debugger, internal_dict):
	print('installing eastl formatters to lldb')

	debugger.HandleCommand('type summary add --summary-string "value=${var.mValue} size=1" eastl::array<>')
	debugger.HandleCommand('type summary add --summary-string "value=${var.mpBegin} size=(${var.mpEnd}-${var.mpBegin})" eastl::VectorBase<>')W

// TODO:
// eastl::unique_ptr<>
// eastl::shared_ptr<>
// eastl::weak_ptr<> 
// eastl::basic_string<> length, capacity, value
// eastl::basic_string<wchar_t> length, capacity, value (don't use this)
// eastl::pair<> first, second
// eastl::span<>
// eastl::DequeBase<>
// eastl::DequeueIterator<>
// eastl::queue<>
// eastl::ListBase<>
// eastl::ListNode<>
// eastl::ListIterator<>
// eastl::SListBase<>
// eastl::SListNode<>
// eastl::SListIterator<>
// eastl::intrusive_list_base<>
// eastl::intrusive_list_iterator<>
// eastl::set<>
// eastl::rbtree<>
// eastl::rbtree_node<>
// eastl::rbtree_iterator<>
// eastl::hashtable<>
// eastl::hash_node<>
// eastl::hashtable_iterator_base<>
// eastl::reverse_iterator<>
// eastl::bitset<>
// eastl::basic_string_view<>
// eastl::compressed_pair_imp<>
// eastl::optional<>
// eastl::ratio<>
// eastl::chrono::duration<> like 7 of thse
// eastl::reference_wrapper<>
// eastl::any<>
// eastl::atomic_flag<>
