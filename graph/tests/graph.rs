use graph::Graph;

#[test]
fn dynamic_edge_indices_match_iteration() {
    let mut graph = Graph::<u16>::new();
    graph.add_fixed_edges(vec![1]);
    graph.add_fixed_edges(vec![0]);

    let first = graph.add_dynamic_edge(0, 0);
    let second = graph.add_dynamic_edge(1, 1);
    let third = graph.add_dynamic_edge(0, 1);

    assert_eq!((first, second, third), (2, 3, 4));
    assert_eq!(
        graph.edges(0).collect::<Vec<_>>(),
        vec![(0, 1), (third, 1), (first, 0)]
    );
    assert_eq!(
        graph.edges(1).collect::<Vec<_>>(),
        vec![(1, 0), (second, 1)]
    );
}

#[test]
fn mixed_edges_preserve_order_and_keep_vertices_independent() {
    let mut graph = Graph::<u16>::new();
    assert_eq!(graph.add_fixed_edges(vec![0, 1, 1]), (0, 3));
    assert_eq!(graph.add_fixed_edges(vec![]), (3, 3));
    assert_eq!(graph.add_fixed_edges(vec![3]), (3, 4));
    assert_eq!(graph.add_fixed_edges(vec![]), (4, 4));
    assert_eq!(graph.add_fixed_edges(vec![]), (4, 4));

    assert_eq!(graph.add_dynamic_edge(1, 1), 4);
    assert_eq!(graph.add_dynamic_edge(0, 1), 5);
    assert_eq!(graph.add_dynamic_edge(2, 0), 6);
    assert_eq!(graph.add_dynamic_edge(1, 1), 7);
    assert_eq!(graph.add_dynamic_edge(0, 0), 8);

    let expected = [
        vec![(0, 0), (1, 1), (2, 1), (8, 0), (5, 1)],
        vec![(7, 1), (4, 1)],
        vec![(3, 3), (6, 0)],
        vec![],
        vec![],
    ];
    for (vertex, edges) in expected.iter().enumerate() {
        assert_eq!(graph.edges(vertex as u16).collect::<Vec<_>>(), *edges);
    }

    let mut edges = graph.edges(1);
    assert_eq!(edges.next(), Some((7, 1)));
    assert_eq!(edges.next(), Some((4, 1)));
    assert_eq!(edges.next(), None);
    assert_eq!(edges.next(), None);
}

#[test]
fn interleaved_insertions_match_an_adjacency_list_model() {
    let mut graph = Graph::<u16>::new();
    let mut fixed = vec![Vec::new(); 12];
    let mut dynamic = vec![Vec::new(); 12];
    let mut next_id = 0;

    for from in 0..12u16 {
        let destinations: Vec<_> = (0..from % 4)
            .map(|offset| (from + offset + 1) % 12)
            .collect();
        for &to in &destinations {
            fixed[from as usize].push((next_id, to));
            next_id += 1;
        }
        graph.add_fixed_edges(destinations);
    }

    for step in 0..80u16 {
        let from = (step * 7) % 12;
        let to = (step * 5 + 3) % 12;
        assert_eq!(graph.add_dynamic_edge(from, to), next_id);
        dynamic[from as usize].push((next_id, to));
        next_id += 1;

        for vertex in 0..12u16 {
            let expected: Vec<_> = fixed[vertex as usize]
                .iter()
                .chain(dynamic[vertex as usize].iter().rev())
                .copied()
                .collect();
            assert_eq!(
                graph.edges(vertex).collect::<Vec<_>>(),
                expected,
                "vertex {vertex} after insertion {step}"
            );
        }
    }
}

#[test]
fn clear_allows_rebuilding_with_fresh_edge_indices() {
    let mut graph = Graph::<u16>::new();
    graph.add_fixed_edges(vec![1, 0]);
    graph.add_fixed_edges(vec![0]);
    graph.add_dynamic_edge(0, 1);
    graph.add_dynamic_edge(1, 1);

    graph.clear();
    graph.clear();
    graph.add_fixed_edges(vec![]);
    graph.add_fixed_edges(vec![2]);
    graph.add_fixed_edges(vec![]);

    assert_eq!(graph.add_dynamic_edge(2, 1), 1);
    assert_eq!(graph.add_dynamic_edge(1, 0), 2);
    assert_eq!(graph.edges(0).collect::<Vec<_>>(), vec![]);
    assert_eq!(
        graph.edges(1).collect::<Vec<_>>(),
        vec![(0, 2), (2, 0)]
    );
    assert_eq!(graph.edges(2).collect::<Vec<_>>(), vec![(1, 1)]);

    graph.clear();
    graph.add_fixed_edges(vec![]);
    assert_eq!(graph.add_dynamic_edge(0, 0), 0);
    assert_eq!(graph.edges(0).collect::<Vec<_>>(), vec![(0, 0)]);
}
