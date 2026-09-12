from osbornex.client import _LOCAL_ID_BITS, make_order_id


def test_order_id_places_source_in_top_bits():
    order_id = make_order_id(source=1, local_id=1)
    assert order_id == (1 << _LOCAL_ID_BITS) | 1


def test_different_sources_never_overlap_for_any_local_id():
    samples = [0, 1, 2, 100, (1 << _LOCAL_ID_BITS) - 1]
    ids_a = {make_order_id(source=1, local_id=local_id) for local_id in samples}
    ids_b = {make_order_id(source=2, local_id=local_id) for local_id in samples}
    assert ids_a.isdisjoint(ids_b)


def test_local_id_is_masked_to_48_bits():
    overflowed = make_order_id(source=0, local_id=(1 << _LOCAL_ID_BITS) + 5)
    assert overflowed == 5
