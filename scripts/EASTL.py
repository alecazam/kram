// manual conversion of EASTL.natvis

// see here for string, table, sequences for Nim language
// https://www.reddit.com/r/nim/comments/lhaaa6/debugging_support_formatters_for_lldb_in_vscode/

// https://pspdfkit.com/blog/2018/how-to-extend-lldb-to-provide-a-better-debugging-experience/debugging_support_formatters_for_lldb_in_vscode
// TODO: size is template param


def __lldb_init_module(debugger, internal_dict):
	print('installing eastl formatters to lldb')

	debugger.HandleCommand('type summary add --summary-string "${var.mValue}" east::array<*>')
	debugger.HandleCommand('type summary add --summary-string "${var.mpBegin} size=${var.mpEnd}-$(var.mpBegin}" east::VectorBase<*>')
