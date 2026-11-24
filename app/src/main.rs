use graph::Graph;

fn main() {
    let mut graph = Graph::<u16>::new();

    graph.add_fixed_edges(vec![1, 2]);
    graph.add_fixed_edges(vec![2]);
    graph.add_fixed_edges(vec![]);
    graph.add_fixed_edges(vec![]);

    println!("Fixed edges (edge ID, destination):");
    for vertex in 0..4 {
        println!("  {vertex}: {:?}", graph.edges(vertex).collect::<Vec<_>>());
    }

    graph.add_dynamic_edge(0, 3);
    graph.add_dynamic_edge(0, 2);
    graph.add_dynamic_edge(2, 0);

    println!("\nCombined edges (fixed first, then dynamic newest first):");
    for vertex in 0..4 {
        println!("  {vertex}: {:?}", graph.edges(vertex).collect::<Vec<_>>());
    }
}
