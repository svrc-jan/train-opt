
use core::{Idx, Ragged};

pub struct DynamicEdge<I: Idx> {
    pub to: I,
    pub next: I,
}

pub struct Graph<I: Idx = u16> {
    fixed_to: Ragged<I, I>,
    dynamic_head: Vec<I>,
    dynamic_edges: Vec<DynamicEdge<I>>,
}

impl<I: Idx> Graph<I> {
    pub fn new() -> Self {
        Self {
            fixed_to: Ragged::new(),
            dynamic_head: Vec::new(),
            dynamic_edges: Vec::new(),
        }
    }

    pub fn add_fixed_edges(&mut self, to: Vec<I>) -> (I, I) {
        debug_assert!(self.dynamic_edges.is_empty());
        
        let range = self.fixed_to.extend_row(to);
        self.dynamic_head.push(I::MAX);

        debug_assert!(self.dynamic_head.len() == self.fixed_to.rows());

        range
    }

    /// Returns the global edge index yielded by `edges`.
    pub fn add_dynamic_edge(&mut self, from: I, to: I) -> I {
        let edge_idx = I::from_usize(self.dynamic_edges.len());
        self.dynamic_edges.push(DynamicEdge { to, next: self.dynamic_head[from.to_usize()] });
        self.dynamic_head[from.to_usize()] = edge_idx;
        I::from_usize(self.fixed_to.elements()) + edge_idx
    }

    pub fn clear(&mut self) {
        self.dynamic_edges.clear();
        self.dynamic_head.clear();
        self.fixed_to.clear();
    }

    pub fn edges(&self, from: I) -> impl Iterator<Item = (I, I)> + '_ {
        let fixed_iter = self.fixed_to.get_enum_copied(from);
        let dynamic_tier = DynamicIter {
            edges: &self.dynamic_edges,
            offset: I::from_usize(self.fixed_to.elements()),
            curr: self.dynamic_head[from.to_usize()]
        };

        fixed_iter.chain(dynamic_tier)
    }


}


pub struct DynamicIter<'a, I: Idx> {
    edges: &'a Vec<DynamicEdge<I>>,
    offset: I,
    curr: I,
}

impl<'a, I: Idx> Iterator for DynamicIter<'a, I> {
    type Item = (I, I);

    fn next(&mut self) -> Option<Self::Item> {
        if self.curr == I::MAX {
            return None;
        }

        let idx = self.curr + self.offset;

        let edge = &self.edges[self.curr.to_usize()];
        self.curr = edge.next;

        Some((idx, edge.to))
    }
}


