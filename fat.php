<?php

    if(!isset($_GET['u']) || !isset($_GET['r']))
    {
        echo "uid (u) or game-id (r) not provided";
        exit;
    }
    $_GET['u'] = intval($_GET['u']);
    $_GET['r'] = intval($_GET['r']);

    if(isset($_GET['p']))
    {
        if(file_exists($_GET['r'] . "/" . $_GET['u']) == false)
        {
            //echo "not registered";
            header("HTTP/1.1 200 OK");
            exit;
        }
        if(strlen($_GET['p']) != 12)
        {
            //echo "wrong size";
            header("HTTP/1.1 200 OK");
            exit;
        }
        file_put_contents($_GET['r'] . "/" . $_GET['u'], $_GET['p'], LOCK_EX);
        $ar = glob($_GET['r'] . '/*');
        foreach($ar as $k)
        {
            if(basename($k) == $_GET['u']){continue;}
            echo file_get_contents($k);
        }
        exit;
    }
    else if(isset($_GET['r']))
    {
        if(time() > $_GET['r'])
        {
            echo "registration rejected: time period expired";
            exit;
        }
        if(is_dir($_GET['r']) == true)
        {
            $ar = glob($_GET['r'] . '/*');
            if(count($ar) >= 8)
            {
                echo "registration rejected: max players reached";
                exit;
            }
        }
        if(file_exists($_GET['r'] . "/" . $_GET['u']) == true)
        {
            echo "registration rejected: already registered";
            exit;
        }
        mkdir($_GET['r']);
        file_put_contents($_GET['r'] . "/" . $_GET['u'], "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", LOCK_EX);
        header("HTTP/1.1 200 OK");
        exit;
    }

?>