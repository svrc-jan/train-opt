use crate::idx::Idx;

pub struct Ragged<T, I: Idx = usize> {
    data: Vec<T>,
    offsets: Vec<I>,
}

impl<T, I: Idx> Ragged<T, I> {
    pub fn new() -> Self {
        Self { data: Vec::new(), offsets: Vec::new() }
    }

    #[inline]
    pub fn push_row(&mut self) -> I {
        let idx = I::from_usize(self.data.len());
        self.offsets.push(idx);
        idx
    }

    #[inline]
    /// Returns the inserted element's global index.
    pub fn push_to_last(&mut self, element: T) -> I {
        assert!(!self.offsets.is_empty(), "Zero rows exist");
        let idx = I::from_usize(self.data.len());
        self.data.push(element);
        idx
    }

    #[inline]
    /// Returns the inserted elements' half-open global range `(start, after)`.
    pub fn extend_last<X>(&mut self, elements: X) -> (I, I)
    where 
        X: IntoIterator<Item = T>
    {
        assert!(!self.offsets.is_empty(), "Zero rows exist");
        let start = I::from_usize(self.data.len());
        self.data.extend(elements);
        (start, I::from_usize(self.data.len()))
    }

    #[inline]
    /// Adds a row and returns its half-open global range `(start, after)`.
    pub fn extend_row<X>(&mut self, elements: X) -> (I, I)
    where 
        X: IntoIterator<Item = T>
    {
        self.push_row();
        self.extend_last(elements)
    }

    #[inline]
    pub fn clear(&mut self) {
        self.data.clear();
        self.offsets.clear();
    }


    #[inline]
    fn row_start(&self, row: usize) -> usize {
        self.offsets[row].to_usize()
    }

    #[inline]
    fn row_end(&self, row: usize) -> usize {
        if row + 1 < self.offsets.len() {
            self.offsets[row + 1].to_usize()
        }
        else {
            self.data.len()
        }
    }

    #[inline]
    pub fn get(&self, row: I) -> &[T] {
        let row_ = row.to_usize();
        
        let start = self.row_start(row_);
        let end = self.row_end(row_);

        &self.data[start..end]
    }


    #[inline]
    pub fn get_enum(&self, row: I) -> impl Iterator<Item = (I, &T)> {
        let row_ = row.to_usize();
        
        let start = self.row_start(row_);
        let end = self.row_end(row_);

        self.data[start..end]
            .iter()
            .enumerate()
            .map(move |(i, x)| (I::from_usize(i + start), x))
    }


    #[inline]
    pub fn get_enum_copied(&self, row: I) -> impl Iterator<Item = (I, T)> + '_
    where
    T: Copy,
    {
        let row_ = row.to_usize();
        let start = self.row_start(row_);
        let end = self.row_end(row_);

        (start..end)
            .zip(&self.data[start..end])
            .map(|(idx, &val)| (I::from_usize(idx), val))
    }

    #[inline]
    pub fn get_mut(&mut self, row: usize) -> &mut [T] {
        assert!(row < self.offsets.len(), "Row index out of bounds");

        let start = self.offsets[row].to_usize();
        let end;

        if row + 1 < self.offsets.len() {
            end = self.offsets[row + 1].to_usize();
        }
        else {
            end = self.elements();  
        }

        &mut self.data[start..end]
    }

    #[inline]
    pub fn rows(&self) -> usize {
        self.offsets.len()
    }
    
    #[inline]
    pub fn elements(&self) -> usize {
        self.data.len()
    }
}


