--TEST--
swoole_server/ssl: heartbeat with bad client
--SKIPIF--
<?php
require __DIR__ . '/../../include/skipif.inc';
skip_if_in_valgrind();
skip_if_darwin_todo();
?>
--FILE--
<?php
require __DIR__ . '/../../include/bootstrap.php';

use Swoole\Client;
use Swoole\Process;
use Swoole\Server;
use SwooleTest\ProcessManager;

$pm = new ProcessManager();
$pm->parentFunc = function ($pid) use ($pm) {
    $client = new Client(SWOOLE_SOCK_TCP, SWOOLE_SOCK_SYNC);
    if (!$client->connect('127.0.0.1', $pm->getFreePort(), 5, 0)) {
        echo 'Over flow. errno=' . $client->errCode;
        exit("\n");
    }
    $s1 = time();
    Assert::same($client->recv(), '');
    $s2 = time();
    Assert::assert($s2 - $s1 > 1);
    Process::kill($pid);
};

$pm->childFunc = function () use ($pm) {
    $serv = new Server('127.0.0.1', $pm->getFreePort(), SWOOLE_PROCESS, SWOOLE_SOCK_TCP | SWOOLE_SSL);
    $serv->set([
        'heartbeat_check_interval' => 1,
        'log_file' => '/dev/null',
        'heartbeat_idle_time' => 1,
        'ssl_cert_file' => SSL_FILE_DIR . '/server-cert.pem',
        'ssl_key_file' => SSL_FILE_DIR . '/server-key.pem',
    ]);
    $serv->on('workerStart', function ($serv) use ($pm) {
        $pm->wakeup();
    });
    $serv->on('receive', function (Server $serv, $fd, $rid, $data) {});
    $serv->start();
};

$pm->childFirst();
$pm->run();
?>
--EXPECT--
