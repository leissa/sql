-- Column types, including the non-reserved `text` and the multi-word character types.
CREATE TABLE t (
    a integer,
    b INT,
    c SMALLINT,
    d BIGINT,
    e BOOLEAN,
    f DATE,
    g REAL,
    h DOUBLE PRECISION,
    q CHARACTER LARGE OBJECT,
    r BINARY LARGE OBJECT,
    i FLOAT,
    j NUMERIC(10, 2),
    k DECIMAL(8),
    l CHARACTER VARYING(255),
    m VARCHAR(12),
    n CHAR(1),
    o text,
    p uuid
);

-- Column constraints, in and out of order.
CREATE TABLE u (
    id integer NOT NULL PRIMARY KEY,
    parent integer REFERENCES u (id),
    kind CHARACTER VARYING(32) NOT NULL UNIQUE,
    amount integer DEFAULT 0,
    nullable integer NULL
);

-- Table-level constraints, named and anonymous.
CREATE TABLE v (
    a integer,
    b integer,
    c integer,
    PRIMARY KEY (a, b),
    UNIQUE (c),
    CHECK (a > 0),
    CONSTRAINT v_fk FOREIGN KEY (c) REFERENCES u (id)
);
