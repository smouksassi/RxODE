context("Test event Table");

et <- eventTable();
test_that("Empty event table check", {
    sink("test");
    print(et)
    sink();
    p1 <- readLines("test");
    unlink("test");
    expect_equal(gsub("^ +", " ", p1), c("EventTable with 0 records:", " 0 dosing (in NA) records", " 0 observation time (in hours) records"))
    expect_equal(et$get.EventTable(), NULL);
    expect_equal(et$get.dosing(), NULL);
    expect_equal(et$get.sampling(), NULL);
})

et$add.dosing(30, amount.units="mg")

test_that("One dose of 30 mg", {
    sink("test");
    print(et)
    sink();
    p1 <- readLines("test");
    unlink("test");
    expect_equal(gsub("^ +", " ", p1), c("EventTable with 1 records:", " 1 dosing (in mg) records", " 0 observation time (in hours) records"))
    expect_equal(et$get.EventTable(), data.frame(time=0, evid=101, amt=30));
    expect_equal(et$get.dosing(), data.frame(time=0, evid=101, amt=30));
    expect_equal(length(et$get.sampling()$time), 0);
})

et$add.sampling(seq(1, 24))

test_that("Adding sampling every hour works", {
    sink("test");
    print(et)
    sink();
    p1 <- readLines("test");
    unlink("test");
    expect_equal(gsub("^ +", " ", p1), c("EventTable with 25 records:", " 1 dosing (in mg) records", " 24 observation time (in hours) records"))
    expect_equal(et$get.EventTable(), rbind(data.frame(time=0, evid=101, amt=30),
                                            data.frame(time=1:24, evid=0, amt=NA)));
    expect_equal(et$get.dosing(), data.frame(time=0, evid=101, amt=30));
    tmp <- data.frame(time=1:24, evid=0);
    row.names(tmp) <- seq(2, 25);
    expect_equal(et$get.sampling()[1:2], tmp);
    expect_true(all(is.na(et$get.sampling()$amt)))
})

test_that("Cannot add units that don't make sense.'", {
    expect_error(et$add.dosing(30, start.time=30, amount.units="ng"),
                 "dosing units differ from EventTable's")
    expect_error(et$add.sampling(seq(0, 24), time.units="secs"), "time units differ from EventTable's")
})

et$clear.sampling()

test_that("Clear sampling works!", {
    sink("test");
    print(et)
    sink();
    p1 <- readLines("test");
    unlink("test");
    expect_equal(gsub("^ +", " ", p1), c("EventTable with 1 records:", " 1 dosing (in mg) records", " 0 observation time (in hours) records"))
    expect_equal(et$get.EventTable(), data.frame(time=0, evid=101, amt=30));
    expect_equal(et$get.dosing(), data.frame(time=0, evid=101, amt=30));
    expect_equal(length(et$get.sampling()$time), 0);
})

et$add.sampling(1);

test_that("Add one sampling", {
    sink("test");
    print(et)
    sink();
    p1 <- readLines("test");
    unlink("test");
    expect_equal(gsub("^ +", " ", p1), c("EventTable with 2 records:", " 1 dosing (in mg) records", " 1 observation time (in hours) records"))
    expect_equal(et$get.EventTable(), rbind(data.frame(time=0, evid=101, amt=30),
                                            data.frame(time=1, evid=0, amt=NA)));
    expect_equal(et$get.dosing(), data.frame(time=0, evid=101, amt=30));
    expect_equal(et$get.sampling()$time, 1);
    expect_equal(et$get.sampling()$evid, 0);
})

et$clear.dosing()

test_that("Clear dosing works!", {
    sink("test");
    print(et)
    sink();
    p1 <- readLines("test");
    unlink("test");
    expect_equal(gsub("^ +", " ", p1), c("EventTable with 1 records:", " 0 dosing (in mg) records", " 1 observation time (in hours) records"))
    expect_equal(et$get.EventTable()[, 1:2], data.frame(time=1, evid=0));
    expect_equal(length(et$get.dosing()$time), 0);
    expect_equal(et$get.sampling()[, 1:2], data.frame(time=1, evid=0));
})