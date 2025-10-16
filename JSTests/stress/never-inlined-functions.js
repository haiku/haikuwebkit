function shouldBe(a, b) {
    if (a !== b) {
        throw new Error(`Expected ${b} but got ${a}`);
    }
}

const regularFunction = function () {};
const promiseReactionJob = $vm.createBuiltin("(function () { return @promiseReactionJob })")();
const promiseReactionJobWithoutPromise = $vm.createBuiltin("(function () { return @promiseReactionJobWithoutPromise })")();

shouldBe($vm.isNeverInline(regularFunction), false);
shouldBe($vm.isNeverInline(promiseReactionJob), true);
shouldBe($vm.isNeverInline(promiseReactionJobWithoutPromise), true);
