use std::fmt::Debug;
use std::hash::Hash;
use std::ops::Add;


mod private {
    pub trait Sealed {}
    impl Sealed for u8 {}
    impl Sealed for u16 {}
    impl Sealed for u32 {}
    impl Sealed for u64 {}
    impl Sealed for usize {}
}

pub trait Idx: private::Sealed + Copy + Eq + Add<Output = Self> + Ord + Hash + Debug + Default {
    const ZERO: Self;
    const MAX: Self;

    fn to_usize(self) -> usize;
    fn from_usize(value: usize) -> Self;
}


macro_rules! impl_idx {
    ($t:ty) => {
        impl Idx for $t {
            const ZERO: Self = 0;
            const MAX: Self = <$t>::MAX;

            fn to_usize(self) -> usize {
                self as usize
            }

            fn from_usize(value: usize) -> Self {
                value as $t
            }
        }
    };
}

impl_idx!(u8);
impl_idx!(u16);
impl_idx!(u32);
impl_idx!(u64);
impl_idx!(usize);
