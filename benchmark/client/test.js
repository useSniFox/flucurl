async function test() {
    console.log("Test nodejs");
    let time = new Date().getTime();
    var url = "http://localhost:8080/size/1";
    for (let i = 0; i < 10000; i++) {
        let res = await fetch(url);
        await res.arrayBuffer();
    }
    console.log(`Small file time: ${new Date().getTime() - time}ms`);
    url = "http://localhost:8080/size/10000";
    time = new Date().getTime();
    for (let i = 0; i < 100; i++) {
        let res = await fetch(url);
        await res.arrayBuffer();
    }
    console.log(`Large file time: ${new Date().getTime() - time}ms`);
}

test();