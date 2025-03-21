function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

(function () {
    function indexOf(array, value)
    {
        return array.indexOf(value);
    }
    noInline(indexOf);

    var array = [];
    for (var i = 0; i < 100; ++i)
        array.push(String(i));

    for (var i = 0; i < testLoopCount; ++i)
        shouldBe(indexOf(array, "42"), 42);
}());

(function () {
    function indexOf(array, value)
    {
        return array.indexOf(value);
    }
    noInline(indexOf);

    var array = [];
    for (var i = 0; i < 100; ++i) {
        array.push(String(i + 0.5));
        array.push({});
    }

    for (var i = 0; i < testLoopCount; ++i)
        shouldBe(indexOf(array, "42.5"), 42 * 2);
}());
