# Visual X# executable specifications

The `.vxs` files in this directory are explanatory Visual X# example suites. Their location under `Spec/`, numbered examples, and explicit expectation comments distinguish specification fragments from application sources.

Each example records:

- the renewed design section it illustrates;
- the rule or behavior demonstrated by the fragment;
- whether the fragment is expected to compile or be rejected; and
- whether omitted declarations are required to supply the surrounding context.

The files are not concatenated programs. Repeated declarations and deliberately invalid fragments are intentional because every numbered example is an independent specification case.

Implementation work should use these `.vxs` examples together with the completed normative language documents, rather than infer semantics from historical source examples.
