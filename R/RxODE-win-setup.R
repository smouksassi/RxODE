##' Returns a list of physical drives that have been or currently are
##' mounted to the computer.
##'
##' This excludes network drives.  See
##' \url{https://www.forensicmag.com/article/2012/06/windows-7-registry-forensics-part-5}
##'
##' @param duplicates Return drives with duplicate entires in
##'     \code{SYSTEM\\MountedDevices}; These are likely removable media.  By default this is \code{FALSE}
##' @return Drives with letters
##' @author Matthew L. Fidler
##' @keywords internal
##' @export
rxPhysicalDrives <- memoise::memoise(function(duplicates=FALSE){
    if(.Platform$OS.type == "unix"){
        return(NULL)
    } else {
        ## This lists all the drive letters (and volume
        ## information) of drives mounted to your computer.
        n <- names(utils::readRegistry("SYSTEM\\MountedDevices"))
        reg <- rex::rex(start, "\\DosDevices\\", capture(or("A":"Z", "a":"z"), ":"), end)
        ns <- n[regexpr(reg, n) != -1];
        if (length(n) > 0){
            ns <- toupper(gsub(reg, "\\1", ns));
            dups <- unique(ns[duplicated(ns)]);
            if (length(dups) > 1){
                if (duplicates){
                    ## Duplicate drive names are more likely to be removable media letters (like usb/cd/etc.)
                    d <- paste0(sort(unique(dups)), "\\")
                    w <- which(!sapply(d, removableDrive))
                    if (length(d) >= 1){
                        return(d)
                    } else {
                        return("C:\\")
                    }
                } else {
                    d <- paste0(sort(unique(ns[!(ns %in% dups)])), "\\");
                    w <- which(!sapply(d, removableDrive))
                    d <- d[w]
                    if (length(d) >= 1){
                        return(d)
                    } else {
                        return("C:\\")
                    }
                }
            } else {
                d <- paste0(sort(ns), "\\");
                w <- which(!sapply(d, removableDrive));
                d <- d[w]
                if (length(d) >= 1){
                    return(d)
                } else {
                    return("C:\\")
                }
            }
            ret <- ns;
        } else {
            ret <- "C:\\";
        }
        return(ret)
    }
})

rxPythonBaseWin <- function(){
    if(.Platform$OS.type == "unix"){
    } else {
        keys <- NULL;
        keys <- try(utils::readRegistry(sprintf("SOFTWARE\\nlmixr%s", ifelse(.Platform$r_arch == "i386", "32", "")), hive = "HCU", maxdepth = 2), silent = TRUE);
        if (!inherits(keys, "try-error") && dir.exists(normalizePath(file.path(keys[[1]], "python"), winslash="/"))){
            python.base <- normalizePath(file.path(keys[[1]], "python"), winslash="/")
        } else {
            keys <- NULL
            keys <- try(utils::readRegistry("SOFTWARE\\Python\\PythonCore", hive = "HCU", ## view = "32-bit",
                                            maxdepth = 3), silent=TRUE);
            if (is.null(keys) || length(keys) == 0 || inherits(keys, "try-error")){
                try(keys <- utils::readRegistry("SOFTWARE\\Python\\PythonCore", hive = "HLM", ## view = "32-bit",
                                                maxdepth = 3), silent = TRUE);
            }
            python.base <- NULL
            for (i in seq_along(keys)){
                try(python.base <- keys[[i]]$InstallPath$`(Default)`, silent=TRUE)
                if (!is.null(python.base)){
                    if (file.exists(file.path(python.base, "python.exe"))){
                        break;
                    } else {
                        python.base <- NULL;
                    }
                }
            }
        }
        if (!is.null(python.base)){
            python.base <- gsub("\\", "/", utils::shortPathName(gsub(rex::rex(any_of("/", "\\"), end), "", python.base)), fixed=TRUE);
        }
        return(python.base)
    }
}

##' Return Rtools base
##'
##' @return Rtools base path, or "" on unix-style platforms.
##' @author Matthew L. Fidler
rxRtoolsBaseWin <- (function(){
    if(.Platform$OS.type == "unix"){
        return("");
    } else {
        ## Prefer nlmixr rtools over everything
        keys <- NULL
        keys <- try(utils::readRegistry(sprintf("SOFTWARE\\nlmixr%s", ifelse(.Platform$r_arch == "i386", "32", "")), hive = "HCU", maxdepth = 2), silent = TRUE);
        if (!inherits(keys, "try-error")){
            rtools.base <- normalizePath(file.path(keys[[1]], "rtools"), winslash="/")
        } else {
            ## The grep solution assumes that the path is setup correctly;
            gcc <- Sys.which("gcc.exe")
            rtools <- sub("[/\\](mingw).*", "", gcc);
            if (file.exists(file.path(rtools, "Rtools.txt"))){
                return(rtools)
            } else {
                ## Rtools doesn't add itself to the path by default.  To
                ## remove install headaches, fish for the path a bit.

                ## The general solution also corrects the problem of
                ## having msys or cygwin compilers on top of the Rtools
                ## compiler, and will adjust the path (just because which
                ## shows a different path doesn't mean Rtools isn't
                ## there.)
                ## This is what Rtools installer is supposed to do.  There is some discussion on devtools if this really occurs...
                rtools.base <- "C:/Rtools";
                if (!file.exists(rtools.base)){
                {
                    keys <- try(utils::readRegistry("SOFTWARE\\R-core\\Rtools", hive = "HCU", view = "32-bit", maxdepth = 2), silent = TRUE)
                    if (is.null(keys) || length(keys) == 0)
                        keys <- try(utils::readRegistry("SOFTWARE\\R-core\\Rtools", hive = "HLM", view = "32-bit", maxdepth = 2), silent = TRUE)
                    if (!inherits(keys, "try-error")){
                        for(i in seq_along(keys)) {
                            version <- names(keys)[[i]]
                            key <- keys[[version]]
                            if (!is.list(key) || is.null(key$InstallPath)) next;
                            install_path <- normalizePath(key$InstallPath, mustWork = FALSE, winslash = "/");
                            if (file.exists(install_path)){
                                rtools.base <- install_path;
                            }
                        }
                    }
                }
                }
                ver <- R.Version();
                ver <- paste0(ver$major, ".", gsub(rex::rex(start, capture(except_any_of(".")), ".", anything, end), "\\1", ver$minor))
                if (!file.exists(rtools.base)){## Based on Issue #2, Rtools may also be installed to RBuildTools;  This is also reflected on the R-stan website.
                    rtoolslist <- apply(expand.grid(c("Rtools", paste0("Rtools/", ver), "RBuildTools", paste0("RBuildTools/", ver)), rxPhysicalDrives()),1,
                                        function(x){ paste0(x[2], x[1])});
                    for (path in rtoolslist){
                        if (file.exists(path)){
                            return(path)
                        }
                    }
                    ## This way avoid's R's slow for loop, but calculates all file.exists.  I think it may still be slower than a for loop that terminates early
                    ## rtools.base <- rtoolslist[which(file.exists(rtoolslist, sep=""))]
                }
                if (file.exists(rtools.base)){
                    return(rtools.base)
                } else if (file.exists(rtools)) {
                    message("gcc available, assuming it comes from rtools...\nRxODE may not work with other compilers.\n")
                    return(rtools)
                } else {
                    message("This package requires Rtools!\nPlease download from http://cran.r-project.org/bin/windows/Rtools/,\ninstall and restart your R session before proceeding.")
                    return("c:/Rtools")
                }
            }
        }
    }
})
##' Setup Rtools path
##'
##' @param rm.rtools Remove the Rtools from the current path specs.
##' @param rm.python Remove Python from the current path specs.
##'
##' @author Matthew L. Fidler
rxWinRtoolsPath <- function(rm.rtools=TRUE, rm.python=TRUE){
    ## Note that devtools seems to assume that rtools/bin is setup appropriately, and figures out the c compiler from there.
    if(.Platform$OS.type == "unix"){
        return(TRUE)
    } else {
        path <- unique(sapply(sub(rex::rex('"', end), "", sub(rex::rex(start,'"'), "",
                                   gsub("/", "\\\\", strsplit(Sys.getenv("PATH"), ";")[[1]]))),
                              function(x){
            if (file.exists(x)){
                return(normalizePath(x));
            } else {
                return("");
            }
        }))
        path <- path[path != ""];
        if (rm.rtools){
            path <- path[regexpr(rex::rex(or("Rtools", "RTOOLS", "rtools")), path) == -1]
        }
        if (rm.python){
            path <- path[regexpr(rex::rex(or("Python", "python", "PYTHON")), path) == -1]
        }
        r.path <- normalizePath(file.path(Sys.getenv("R_HOME"),paste0("bin",Sys.getenv("R_ARCH"))));
        path <- c(r.path, path);

        ## Look in the registry...
        ## This is taken from devtools and adapted.
        rtools.base <- rxRtoolsBaseWin();
        x <- file.path(rtools.base, ifelse(.Platform$r_arch == "i386","mingw_32/bin", "mingw_64/bin"));
        if (file.exists(x)){
            Sys.setenv(BINPREF=gsub("([^/])$", "\\1/", gsub("\\\\", "/", normalizePath(x))));
        }
        if (file.exists(rtools.base)){
            gcc <- list.files(rtools.base, "gcc",full.names=TRUE)[1]
            if (is.na(gcc)){
                gcc <- "";
            }
            ## This allows both toolchains to be present, but RxODE should still work...
            for (x in rev(c(file.path(rtools.base, "bin"),
                            ## file.path(rtools.base, "mingw_32/bin") ## Rtools sets up the mingw_32/bin first (even if x64)
                            file.path(rtools.base, ifelse(.Platform$r_arch == "i386","mingw_32/bin", "mingw_64/bin")),
                            file.path(rtools.base, ifelse(.Platform$r_arch == "i386","mingw_32/bin", "mingw_64/bin")),
                            file.path(rtools.base, ifelse(.Platform$r_arch == "i386","mingw_32/opt/bin", "mingw_64/opt/bin"))
                            ## ifelse(gcc == "", "", file.path(gcc, "bin")),
                            ## ifelse(gcc == "", "", ifelse(.Platform$r_arch == "i386",file.path(gcc, "bin32"), file.path(gcc, "bin64"))
                            ## )
                            ))){
                if (file.exists(x)){
                    path <- c(normalizePath(x), path);
                }
            }
            python.base <- rxPythonBaseWin();
            if (!is.null(python.base)){
                python <- normalizePath(file.path(python.base, "python.exe"));
                if (file.exists(python)){
                    ## Sometimes there are 2 competing python
                    ## installs.  Make sure to setup everything, just
                    ## in case... Otherwise it can crash R :(
                    Sys.setenv(PYTHON_EXE=normalizePath(python)); ## For PythonInR
                    path <- c(normalizePath(python.base), path);
                    Sys.setenv(PYTHONHOME=normalizePath(python.base));
                    Sys.setenv(PYTHON_INCLUDE=normalizePath(file.path(python.base, "include")));
                    python.lib.base <- normalizePath(file.path(python.base, "libs"))
                    python.lib.name <- list.files(python.lib.base, "[0-9][0-9]+\\.lib$");
                    if (length(python.lib.name) == 1){
                        Sys.setenv(PYTHON_LIB=normalizePath(file.path(python.base, "libs", python.lib.name)));
                    } else {
                        Sys.unsetenv("PYTHON_LIB")
                    }
                    Sys.setenv(PYTHONPATH=paste(c(normalizePath(file.path(python.base, "DLLs")),
                                                  normalizePath(file.path(python.base, "Lib")),
                                                  normalizePath(file.path(python.base, "Lib", "site-packages"))),
                                                collapse=";"));
                    Sys.unsetenv("PYTHONSTARTUP");
                }
            }
            ## java <- as.vector(Sys.which("java"));
            ## if (java != ""){
            ##     java <- sub(rex::rex(one_of("/", "\\"), except_any_of("/", "\\", "\n"), end), "", java)
            ## }
            keys <- NULL;
            ## Is there a 64 bit aspell that should be checked for...?
            keys <- try(utils::readRegistry("SOFTWARE\\Aspell", hive="HLM", view="32-bit", maxdepth=3), silent=TRUE);
            ## Add aspell for cran checks...
            if (!is.null(keys)){
                if (any(names(keys) == "Path")){
                    if (file.exists(keys$Path)){
                        path <- c(normalizePath(keys$Path), path);
                    }
                }
            }
            ## Last Cran check for Rtools is qpdf
            qpdf <- c(paste0(rtools.base, "/qpdf/bin"), paste0(rxPhysicalDrives(), "/qpdf/bin"))
            for (p in qpdf){
                if (file.exists(p)){
                    path <- c(normalizePath(p), path);
                    break;
                }
            }
            path <- path[path != ""];
            path <- paste(unique(path), collapse=";");
            Sys.setenv(PATH=path);
            return(TRUE);
        } else {
            return(FALSE)
        }
    }
}
##' Setup Python and SymPy for windows
##'
##' @author Matthew L. Fidler
##' @export

rxWinPythonSetup <- function(){
    base <- rxPythonBaseWin()
    if (is.null(base)){
        stop("RxODE requires Python. Please install an appropriate version and add it to the system path.")
    }
    if (file.access(paste(base, "/Lib/site-packages", sep=""),2)==-1){
      stop("The Python library path does not appear to be writeable. Please rectify this situation, restart R, and try again.")
    }
    message("Attempting to install SymPy. This may take a few seconds...")
    try(system("python -m pip install sympy"))

    if (!requireNamespace("SnakeCharmR", quietly = TRUE)){
        message("Attempting to install SnakeCharmR. This may take a few seconds...")
        devtools::install_github("nlmixrdevelopment/SnakeCharmR");
    }
    message("Please restart your R session before using RxODE.")
}

##' Setup Windows components for RxODE
##'
##' @inheritParams rxWinRtoolsPath
##' @author Matthew L. Fidler
##' @export
rxWinSetup <- function(rm.rtools=TRUE, rm.python=TRUE){
    if (!rxWinRtoolsPath(rm.rtools, rm.python)){
        message("This package requires Rtools!\nPlease download from http://cran.r-project.org/bin/windows/Rtools/,\ninstall and restart your R session before proceeding.\n");
        # cat("Currently downloading https://cran.r-project.org/bin/windows/Rtools/Rtools33.exe...\n");
        # rtools <- tempfile(fileext="-Rtools33.exe");
        # cat("Downloading to", rtools, "\n");
        # rxWget("http://cran.r-project.org/bin/windows/Rtools/Rtools33.exe", rtools);
        # system(rtools);
        # unlink(rtools);
        # if (!rxWinRtoolsPath(rm.rtools)){
        #     stop("Rtools not setup correctly.");
        # }
    }
    rxWinPythonSetup();
}
