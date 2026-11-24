use core::Ragged;

#[test]
fn insertions_return_global_indices_and_ranges() {
    let mut ragged = Ragged::<_, u16>::new();
    assert_eq!(ragged.extend_row([10, 20]), (0, 2));
    ragged.push_row();
    assert_eq!(ragged.push_to_last(30), 2);
    assert_eq!(ragged.extend_last([40, 50]), (3, 5));
    assert_eq!(ragged.get(0), &[10, 20]);
    assert_eq!(
        ragged.get_enum_copied(1).collect::<Vec<_>>(),
        vec![(2, 30), (3, 40), (4, 50)]
    );
}

#[test]
fn empty_extensions_return_equal_bounds() {
    let mut ragged = Ragged::<i32, u16>::new();
    assert_eq!(ragged.extend_row([]), (0, 0));
    assert_eq!(ragged.push_to_last(10), 0);
    assert_eq!(ragged.extend_last([]), (1, 1));
    assert_eq!(ragged.extend_row([]), (1, 1));
    assert_eq!(ragged.get(0), &[10]);
    assert!(ragged.get(1).is_empty());
}
