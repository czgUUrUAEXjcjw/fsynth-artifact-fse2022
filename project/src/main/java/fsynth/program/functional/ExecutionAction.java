package fsynth.program.functional;

import fsynth.program.subject.ExecutionInfo;

/**
 * @author anonymous
 * @since 2019-06-04
 **/
@FunctionalInterface
public interface ExecutionAction {
    /**
     * @param action Execution Info from the subject
     */
    void run(ExecutionInfo action);
}
