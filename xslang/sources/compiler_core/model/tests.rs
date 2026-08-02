#[cfg(test)]
mod tests
{
    use super::*;

    fn node(parent_index: u64, first_child: u64, child_count: u64, text_offset: u64, text_length: u64)
    -> RawSyntaxNode
    {
        RawSyntaxNode {
            kind: 0,
            token_kind: 0,
            visibility: 0,
            flags: 0,
            parent_index,
            first_child,
            child_count,
            text_offset,
            text_length,
            file_id: 7,
            start_offset: 0,
            end_offset: 4,
            start_line: 1,
            start_column: 1,
            end_line: 1,
            end_column: 5,
        }
    }

    #[test]
    fn validates_and_reads_flat_syntax_packet()
    {
        let nodes = [node(NO_NODE, 0, 1, 0, 4), node(0, 1, 0, 4, 4)];
        let children = [1];
        let text = b"filemain";
        let packet = RawSyntaxPacket {
            abi_version: SYNTAX_ABI_VERSION,
            reserved: 0,
            root_index: 0,
            nodes: nodes.as_ptr(),
            node_count: nodes.len() as u64,
            child_indices: children.as_ptr(),
            child_index_count: children.len() as u64,
            text_bytes: text.as_ptr(),
            text_byte_count: text.len() as u64,
        };
        // SAFETY: All packet tables borrow live local arrays with matching lengths.
        let view = unsafe { SyntaxPacketView::from_raw(&packet) }.expect("valid syntax packet");
        assert_eq!(view.root_index(), 0);
        assert_eq!(view.children(0), Ok(&children[..]));
        assert_eq!(view.node_text(1), Ok("main"));
        let owned = view.to_owned_tree().expect("owned syntax tree");
        assert_eq!(owned.root, 0);
        assert_eq!(owned.nodes[0].children, vec![1]);
        assert_eq!(owned.nodes[1].parent, Some(0));
        assert_eq!(owned.nodes[1].text, "main");
    }

    #[test]
    fn raw_layout_matches_c23_abi()
    {
        assert_eq!(std::mem::size_of::<RawSyntaxNode>(), 112);
        assert_eq!(std::mem::offset_of!(RawSyntaxNode, parent_index), 16);
        assert_eq!(std::mem::offset_of!(RawSyntaxNode, end_column), 104);
        assert_eq!(std::mem::size_of::<RawSyntaxPacket>(), 64);
        assert_eq!(std::mem::offset_of!(RawSyntaxPacket, nodes), 16);
        assert_eq!(std::mem::offset_of!(RawSyntaxPacket, text_byte_count), 56);
    }

    #[test]
    fn rejects_wrong_version_and_child_parent()
    {
        let nodes = [node(NO_NODE, 0, 1, 0, 0), node(NO_NODE, 1, 0, 0, 0)];
        let children = [1];
        let packet = RawSyntaxPacket {
            abi_version: SYNTAX_ABI_VERSION + 1,
            reserved: 0,
            root_index: 0,
            nodes: nodes.as_ptr(),
            node_count: 2,
            child_indices: children.as_ptr(),
            child_index_count: 1,
            text_bytes: std::ptr::null(),
            text_byte_count: 0,
        };
        // SAFETY: All non-empty packet tables borrow live local arrays.
        assert_eq!(
            unsafe { SyntaxPacketView::from_raw(&packet) }.unwrap_err(),
            PacketError::UnsupportedVersion
        );
        let packet = RawSyntaxPacket {
            abi_version: SYNTAX_ABI_VERSION,
            ..packet
        };
        // SAFETY: All non-empty packet tables borrow live local arrays.
        assert_eq!(
            unsafe { SyntaxPacketView::from_raw(&packet) }.unwrap_err(),
            PacketError::InvalidChildParent
        );
    }

    #[test]
    fn ffi_session_owns_valid_packet_and_rejects_invalid_input()
    {
        let nodes = [node(NO_NODE, 0, 0, 0, 4)];
        let text = b"file";
        let packet = RawSyntaxPacket {
            abi_version: SYNTAX_ABI_VERSION,
            reserved: 0,
            root_index: 0,
            nodes: nodes.as_ptr(),
            node_count: 1,
            child_indices: std::ptr::null(),
            child_index_count: 0,
            text_bytes: text.as_ptr(),
            text_byte_count: text.len() as u64,
        };
        let mut session = std::ptr::null_mut();
        // SAFETY: Packet tables borrow live local arrays and the output is writable.
        assert_eq!(
            unsafe { xslang_compiler_core_session_create(&packet, &raw mut session) },
            FfiStatus::Ok
        );
        // SAFETY: The returned session remains live.
        assert_eq!(unsafe { xslang_compiler_core_session_syntax_node_count(session) }, 1);
        // SAFETY: The session came from the matching constructor and is freed once.
        unsafe { xslang_compiler_core_session_free(session) };
        // SAFETY: Null input is rejected before dereferencing.
        assert_eq!(
            unsafe { xslang_compiler_core_session_create(std::ptr::null(), &raw mut session) },
            FfiStatus::NullArgument
        );
        let invalid = RawSyntaxPacket {
            abi_version: SYNTAX_ABI_VERSION + 1,
            ..packet
        };
        // SAFETY: Packet tables remain live; the version is intentionally invalid.
        assert_eq!(
            unsafe { xslang_compiler_core_session_create(&invalid, &raw mut session) },
            FfiStatus::InvalidPacket
        );
    }
}
