context("Michelis Menton test (long lines)")
## Michelis Menton test
rxPermissive({

    mm <- RxODE({
        C2 = centr / V;
        d/dt(centr)  = -(VM*C2)/(KM+C2);
    })

    mypar3 <- function(lVM, lKM, lV )
    {
        VM = exp(theta[1] + eta[1])
        KM = exp(theta[2] + eta[2])
        V  = exp(theta[3] + eta[3])
    }

    pred <- function() C2

    ## test_that("Functions outside of RxODE/global/nlmixr raise errors", {
    ##     expect_error(rxSymPySetupPred(mm, pred, par, err=function(){err ~ prop(0.1)}, grad=TRUE));
    ## })

    focei.mm.mod2 <- rxSymPySetupPred(mm, pred, mypar3, err=function(){err ~ prop(0.1)}, grad=TRUE, sum.prod=TRUE);

    ## FIXME: should the lines be split?  Logify is one approach, but
    ## perhaps just a split...?
    test_that("long lines are handled...",{
        expect_equal(class(focei.mm.mod2), "rxFocei");
    })

}, silent=TRUE, on.validate=TRUE)
